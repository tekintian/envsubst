/*
 * Production-Grade envsubst with dual mode + wildcard/prefix whitelist
 * Default: ${VAR} only (nginx-safe)
 * --all  : $VAR + ${VAR} (original behavior)
 * Supports prefix wildcards: VAR_*, *_API, ${TEST_*}, $DEV_*
 * @author tekintian@gmail.com
 * @see https://ai.tekin.cn
 * @Repo: https://github.com/tekintian/envsubst/
 */

#define _POSIX_C_SOURCE 200809L
#include <getopt.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ENVSUBST_VERSION "1.0.0-tekin"

struct envsubst_ctx {
    char** allowed_vars;
    size_t var_count;
    bool keep_undefined;
    bool replace_all;
};

struct pattern {
    char* prefix;
    char* suffix;
    bool is_wildcard;
};

static void usage(void) {
    printf("\n");
    printf("envsubst %s - 安全型环境变量替换工具 (支持通配符白名单)\n", ENVSUBST_VERSION);
    printf("Production-Grade envsubst with wildcard & dual replacement mode\n");
    printf("===============================================================\n");
    printf("\n");
    printf("【功能说明】\n");
    printf("  默认模式：仅替换 ${VAR} 格式，保护 Nginx 配置不被破坏\n");
    printf("  --all 模式：同时替换 $VAR 和 ${VAR}，兼容传统 envsubst 行为\n");
    printf("  支持前缀/后缀通配符：REST_*、*_PROD、APP_*_API\n");
    printf("\n");
    printf("【使用方法】\n");
    printf("  envsubst [选项] [变量名/通配符...]\n");
    printf("\n");
    printf("【选项】\n");
    printf("  -h, --help              显示本帮助信息并退出\n");
    printf("  -V, --version           显示版本信息并退出\n");
    printf("  -v, --variables         列出允许替换的变量/通配符并退出\n");
    printf("  -k, --keep-undefined    变量未定义时保留原字符串，不删除\n");
    printf("      --all               启用全替换模式：替换 $VAR 和 ${VAR}\n");
    printf("\n");
    printf("【通配符支持】\n");
    printf("  前缀匹配：REST_*        匹配 REST_HOST、REST_PORT、REST_TOKEN\n");
    printf("  后缀匹配：*_PROD        匹配 DB_PROD、REDIS_PROD\n");
    printf("  前后匹配：APP_*_API     匹配 APP_USER_API、APP_AUTH_API\n");
    printf("\n");
    printf("【使用示例】\n");
    printf("  1. 默认安全替换(仅${VAR})：\n");
    printf("     cat app.conf.tpl | ./envsubst\n");
    printf("\n");
    printf("  2. 全替换模式(传统行为)：\n");
    printf("     cat app.conf.tpl | ./envsubst --all\n");
    printf("\n");
    printf("  3. 白名单通配符替换（推荐容器场景）：\n");
    printf("     ./envsubst 'REST_*' < app.conf.tpl > app.conf\n");
    printf("\n");
    printf("  4. 多组通配符白名单（最常用生产命令）：\n");
    printf("     ./envsubst 'REST_* WAF_* CONFIG_*' < nginx.conf.template > nginx.conf\n");
    printf("\n");
    printf("  5. 容器环境 Nginx 配置标准替换：\n");
    printf("     envsubst < /etc/nginx/nginx.conf.template > /etc/nginx/nginx.conf\n");
    printf("\n");
    printf("  6. 保留未定义变量（推荐容器启动脚本）：\n");
    printf("     envsubst -k 'APP_*' < tpl.conf > out.conf\n");
    printf("\n");
    printf("【项目主页】https://github.com/tekintian/envsubst/\n");
    printf("【联系作者】tekintian@gmail.com\n");
    printf("\n");

    exit(EXIT_SUCCESS);
}

static void version(void) {
    printf("envsubst %s\n", ENVSUBST_VERSION);
    exit(EXIT_SUCCESS);
}

static void* xreallocarray(void* optr, size_t nmemb, size_t elem_size) {
    if (elem_size != 0 && nmemb > (size_t)-1 / elem_size) {
        errno = ENOMEM;
        return NULL;
    }
    return realloc(optr, nmemb * elem_size);
}

static bool is_var_char(int c) {
    return isalnum(c) || c == '_';
}

static char* normalize_plain(const char* s) {
    if (!s) return NULL;
    if (*s == '$') s++;
    if (*s == '{') s++;

    size_t len = 0;
    const char* p = s;
    while (*p && is_var_char((unsigned char)*p)) p++;
    len = p - s;
    if (!len) return NULL;

    char* buf = malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, s, len);
    buf[len] = 0;
    return buf;
}

static char* normalize_brace(const char* s) {
    if (!s || *s != '$' || *(s+1) != '{') return NULL;
    s += 2;
    const char* e = strchr(s, '}');
    if (!e) return NULL;

    size_t len = e - s;
    char* buf = malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, s, len);
    buf[len] = 0;
    return buf;
}

static struct pattern parse_pattern(const char* input) {
    struct pattern pat = {0};
    char* s = normalize_brace(input);
    if (!s) s = normalize_plain(input);
    if (!s) s = strdup(input);

    char* star = strchr(s, '*');
    if (star) {
        pat.is_wildcard = true;
        *star = 0;
        pat.prefix = s;
        pat.suffix = star + 1;
    } else {
        pat.prefix = s;
    }
    return pat;
}

static bool match_pattern(const char* var, struct pattern* pat) {
    if (!pat->is_wildcard) return strcmp(var, pat->prefix) == 0;
    size_t var_len = strlen(var);
    size_t pre_len = strlen(pat->prefix);
    size_t suf_len = strlen(pat->suffix);

    if (var_len < pre_len + suf_len) return false;
    if (strncmp(var, pat->prefix, pre_len) != 0) return false;
    if (suf_len && strcmp(var + var_len - suf_len, pat->suffix) != 0) return false;
    return true;
}

static bool ctx_allow(struct envsubst_ctx* ctx, const char* var) {
    if (!ctx->var_count) return true;
    for (size_t i = 0; i < ctx->var_count; i++) {
        struct pattern p = parse_pattern(ctx->allowed_vars[i]);
        bool ok = match_pattern(var, &p);
        free(p.prefix);
        if (ok) return true;
    }
    return false;
}

static void ctx_init(struct envsubst_ctx* ctx) {
    ctx->allowed_vars = NULL;
    ctx->var_count = 0;
    ctx->keep_undefined = false;
    ctx->replace_all = false;
}

static void ctx_free(struct envsubst_ctx* ctx) {
    for (size_t i = 0; i < ctx->var_count; i++)
        free(ctx->allowed_vars[i]);
    free(ctx->allowed_vars);
}

static int ctx_add(struct envsubst_ctx* ctx, const char* s) {
    char** v = xreallocarray(ctx->allowed_vars, ctx->var_count + 1, sizeof(char*));
    if (!v) return -1;
    ctx->allowed_vars = v;
    ctx->allowed_vars[ctx->var_count++] = strdup(s);
    return 0;
}

static void parse_arg(struct envsubst_ctx* ctx, const char* arg) {
    char* buf = strdup(arg);
    char* save = NULL;
    char* tok = strtok_r(buf, ",", &save);
    for (; tok; tok = strtok_r(NULL, ",", &save))
        ctx_add(ctx, tok);
    free(buf);
}

static void output(FILE* out, struct envsubst_ctx* ctx, const char* var, const char* orig) {
    if (!ctx_allow(ctx, var)) {
        fputs(orig, out);
        return;
    }
    const char* val = getenv(var);
    if (val) fputs(val, out);
    else if (ctx->keep_undefined) fputs(orig, out);
}

static void process_brace(FILE* out, char** p, struct envsubst_ctx* ctx) {
    char* start = *p;
    char* end = strchr(start + 2, '}');
    if (!end) { fputc('$', out); (*p)++; return; }

    char save = *end;
    *end = 0;
    char* var = normalize_brace(start);
    *end = save;

    if (var) {
        output(out, ctx, var, start);
        *p = end;
        free(var);
    } else {
        fputc('$', out);
    }
}

static void process_plain(FILE* out, char** p, struct envsubst_ctx* ctx) {
    char* start = *p;
    char* cur = start + 1;
    while (*cur && is_var_char((unsigned char)*cur)) cur++;

    char save = *cur;
    *cur = 0;
    char* var = normalize_plain(start);
    *cur = save;

    if (var) {
        output(out, ctx, var, start);
        *p = cur - 1;
        free(var);
    } else {
        fputc('$', out);
    }
}

static void process_stream(FILE* in, FILE* out, struct envsubst_ctx* ctx) {
    char* line = NULL;
    size_t sz = 0;
    ssize_t n;

    while ((n = getline(&line, &sz, in)) != -1) {
        for (char* p = line; *p; p++) {
            if (*p == '$') {
                if (*(p+1) == '{')
                    process_brace(out, &p, ctx);
                else if (ctx->replace_all)
                    process_plain(out, &p, ctx);
                else
                    fputc(*p, out);
            } else {
                fputc(*p, out);
            }
        }
    }

    if (ferror(in)) { perror("read"); exit(EXIT_FAILURE); }
    free(line);
}

int main(int argc, char** argv) {
    struct envsubst_ctx ctx;
    ctx_init(&ctx);

    static const struct option opts[] = {
        {"help",    no_argument,       NULL, 'h'},
        {"version", no_argument,       NULL, 'V'},
        {"variables",no_argument,      NULL, 'v'},
        {"keep-undefined",no_argument,  NULL, 'k'},
        {"all",     no_argument,       NULL, 128},
        {0, 0, 0, 0}
    };

    int opt;
    bool list_vars = false;
    while ((opt = getopt_long(argc, argv, "hvVk", opts, NULL)) != -1) {
        switch (opt) {
            case 'h': usage(); break;
            case 'V': version(); break;
            case 'v': list_vars = true; break;
            case 'k': ctx.keep_undefined = true; break;
            case 128: ctx.replace_all = true; break;
            default: usage();
        }
    }

    for (int i = optind; i < argc; i++)
        parse_arg(&ctx, argv[i]);

    if (list_vars) {
        for (size_t i = 0; i < ctx.var_count; i++)
            puts(ctx.allowed_vars[i]);
        ctx_free(&ctx);
        return 0;
    }

    process_stream(stdin, stdout, &ctx);
    ctx_free(&ctx);
    return EXIT_SUCCESS;
}
