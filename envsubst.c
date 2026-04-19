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

#ifndef ENVSUBST_VERSION
#define ENVSUBST_VERSION "v1.0.0"
#endif

struct envsubst_ctx {
    char** allowed_vars;
    size_t var_count;
    bool keep_undefined;
    bool replace_all;
    bool debug_mode;      // 调试模式
    bool stats_mode;      // 统计模式
    bool json_stats;      // JSON 格式统计
    size_t replaced_count;   // 已替换的变量数
    size_t kept_count;       // 保留的未定义变量数
    size_t skipped_count;    // 跳过的变量数（不在白名单中）
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
    printf("      --debug             调试模式：显示每个变量的替换过程\n");
    printf("      --stats             统计模式：显示替换统计信息\n");
    printf("      --json-stats        JSON 格式统计信息（便于机器解析）\n");
    printf("      --whitelist-file F  从文件 F 读取白名单规则（每行一个）\n");
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
    printf("【软件定制开发】https://ai.tekin.cn\n");
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
    
    // Validate variable name
    for (size_t i = 0; i < len; i++) {
        if (!is_var_char((unsigned char)s[i])) {
            return NULL;
        }
    }
    
    char* buf = malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, s, len);
    buf[len] = 0;
    return buf;
}

static struct pattern parse_pattern(const char* input) {
    struct pattern pat = {0};
    
    // Check for wildcard first before normalization
    char* star = strchr(input, '*');
    if (star) {
        pat.is_wildcard = true;
        // Extract prefix (everything before *)
        size_t prefix_len = star - input;
        pat.prefix = malloc(prefix_len + 1);
        if (!pat.prefix) return pat;
        memcpy(pat.prefix, input, prefix_len);
        pat.prefix[prefix_len] = 0;
        
        // Extract suffix (everything after *)
        pat.suffix = strdup(star + 1);
        if (!pat.suffix) {
            free(pat.prefix);
            pat.prefix = NULL;
            return pat;
        }
    } else {
        // No wildcard, normalize the input
        char* s = normalize_brace(input);
        if (!s) s = normalize_plain(input);
        if (!s) s = strdup(input);
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
    ctx->debug_mode = false;
    ctx->stats_mode = false;
    ctx->json_stats = false;
    ctx->replaced_count = 0;
    ctx->kept_count = 0;
    ctx->skipped_count = 0;
}

static void ctx_free(struct envsubst_ctx* ctx) {
    for (size_t i = 0; i < ctx->var_count; i++)
        free(ctx->allowed_vars[i]);
    free(ctx->allowed_vars);
}

static void print_stats(struct envsubst_ctx* ctx) {
    if (!ctx->stats_mode && !ctx->json_stats) return;
    
    if (ctx->json_stats) {
        // JSON format for machine parsing
        fprintf(stderr, "\n{\n");
        fprintf(stderr, "  \"envsubst_stats\": {\n");
        fprintf(stderr, "    \"variables_replaced\": %zu,\n", ctx->replaced_count);
        fprintf(stderr, "    \"variables_kept\": %zu,\n", ctx->kept_count);
        fprintf(stderr, "    \"variables_skipped\": %zu,\n", ctx->skipped_count);
        fprintf(stderr, "    \"total_processed\": %zu\n", 
                ctx->replaced_count + ctx->kept_count + ctx->skipped_count);
        fprintf(stderr, "  }\n");
        fprintf(stderr, "}\n");
    } else {
        // Human-readable format
        fprintf(stderr, "\n=== envsubst Statistics ===\n");
        fprintf(stderr, "Variables replaced:    %zu\n", ctx->replaced_count);
        fprintf(stderr, "Variables kept:        %zu\n", ctx->kept_count);
        fprintf(stderr, "Variables skipped:     %zu\n", ctx->skipped_count);
        fprintf(stderr, "Total processed:       %zu\n", 
                ctx->replaced_count + ctx->kept_count + ctx->skipped_count);
        fprintf(stderr, "========================\n");
    }
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
    // Support both space and comma as delimiters
    char* tok = strtok_r(buf, " ,", &save);
    for (; tok; tok = strtok_r(NULL, " ,", &save))
        ctx_add(ctx, tok);
    free(buf);
}

static int load_whitelist_file(struct envsubst_ctx* ctx, const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open whitelist file '%s': %s\n", 
                filename, strerror(errno));
        return -1;
    }
    
    char* line = NULL;
    size_t len = 0;
    ssize_t nread;
    int count = 0;
    
    while ((nread = getline(&line, &len, fp)) != -1) {
        // Remove trailing newline/carriage return
        while (nread > 0 && (line[nread-1] == '\n' || line[nread-1] == '\r')) {
            line[--nread] = 0;
        }
        
        // Skip empty lines and comments
        if (nread == 0 || line[0] == '#') continue;
        
        // Trim leading whitespace
        char* trimmed = line;
        while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;
        
        if (*trimmed) {
            if (ctx_add(ctx, trimmed) == 0) {
                count++;
            }
        }
    }
    
    free(line);
    fclose(fp);
    
    if (ctx->debug_mode) {
        fprintf(stderr, "[DEBUG] Loaded %d rules from '%s'\n", count, filename);
    }
    
    return count;
}

static void output(FILE* out, struct envsubst_ctx* ctx, const char* var, const char* orig) {
    if (!ctx_allow(ctx, var)) {
        if (ctx->debug_mode) {
            fprintf(stderr, "[DEBUG] Skip (not in whitelist): %s\n", orig);
        }
        ctx->skipped_count++;
        fputs(orig, out);
        return;
    }
    const char* val = getenv(var);
    if (val) {
        if (ctx->debug_mode) {
            fprintf(stderr, "[DEBUG] Replace: %s -> %s\n", orig, val);
        }
        ctx->replaced_count++;
        fputs(val, out);
    } else if (ctx->keep_undefined) {
        if (ctx->debug_mode) {
            fprintf(stderr, "[DEBUG] Keep undefined: %s\n", orig);
        }
        ctx->kept_count++;
        fputs(orig, out);
    } else {
        // Variable is undefined and not kept - output empty string
        if (ctx->debug_mode) {
            fprintf(stderr, "[DEBUG] Remove undefined: %s\n", orig);
        }
        ctx->replaced_count++;  // Count as processed
    }
}

static void process_brace(FILE* out, char** p, struct envsubst_ctx* ctx) {
    char* start = *p;
    char* end = strchr(start + 2, '}');
    if (!end) { 
        fputc('$', out); 
        (*p)++; 
        return; 
    }

    // Calculate the length of the full ${VAR} expression
    size_t expr_len = end - start + 1;
    
    // Extract content between ${ and }
    const char* content = start + 2;
    size_t content_len = end - start - 2;
    
    // Check for default value syntax ${VAR:-default}
    char* var_name = NULL;
    char* default_value = NULL;
    size_t var_len = 0;
    
    // Look for :- pattern
    char* colon_dash = NULL;
    for (size_t i = 0; i < content_len - 1; i++) {
        if (content[i] == ':' && content[i+1] == '-') {
            colon_dash = (char*)content + i;
            break;
        }
    }
    
    if (colon_dash) {
        // Has default value
        var_len = colon_dash - content;
        size_t default_len = content_len - var_len - 2;  // -2 for :-
        
        var_name = malloc(var_len + 1);
        default_value = malloc(default_len + 1);
        
        if (var_name && default_value) {
            memcpy(var_name, content, var_len);
            var_name[var_len] = 0;
            memcpy(default_value, colon_dash + 2, default_len);
            default_value[default_len] = 0;
        } else {
            free(var_name);
            free(default_value);
            var_name = NULL;
            default_value = NULL;
        }
    } else {
        // No default value
        var_len = content_len;
        var_name = malloc(var_len + 1);
        if (var_name) {
            memcpy(var_name, content, var_len);
            var_name[var_len] = 0;
        }
    }
    
    // Validate variable name
    bool valid = true;
    if (var_name && var_len > 0) {
        if (!isalpha((unsigned char)var_name[0]) && var_name[0] != '_') {
            valid = false;
        } else {
            for (size_t i = 1; i < var_len; i++) {
                if (!is_var_char((unsigned char)var_name[i])) {
                    valid = false;
                    break;
                }
            }
        }
    } else {
        valid = false;
    }
    
    if (valid && var_name) {
        // Check whitelist
        if (!ctx_allow(ctx, var_name)) {
            if (ctx->debug_mode) {
                fprintf(stderr, "[DEBUG] Skip (not in whitelist): ${%s}\n", var_name);
            }
            ctx->skipped_count++;
            fwrite(start, 1, expr_len, out);
            free(var_name);
            free(default_value);
            *p = end;
            return;
        }
        
        // Get environment variable value
        const char* env_val = getenv(var_name);
        const char* final_value = env_val;
        
        // If not set and has default, use default
        if (!env_val && default_value) {
            final_value = default_value;
            if (ctx->debug_mode) {
                fprintf(stderr, "[DEBUG] Use default: ${%s:-%s} -> %s\n", 
                        var_name, default_value, default_value);
            }
        }
        
        if (final_value) {
            if (ctx->debug_mode && !default_value) {
                fprintf(stderr, "[DEBUG] Replace: ${%s} -> %s\n", var_name, final_value);
            }
            ctx->replaced_count++;
            fputs(final_value, out);
        } else if (ctx->keep_undefined) {
            if (ctx->debug_mode) {
                fprintf(stderr, "[DEBUG] Keep undefined: ${%s}\n", var_name);
            }
            ctx->kept_count++;
            fwrite(start, 1, expr_len, out);
        } else {
            if (ctx->debug_mode) {
                fprintf(stderr, "[DEBUG] Remove undefined: ${%s}\n", var_name);
            }
            ctx->replaced_count++;
        }
        
        free(var_name);
        free(default_value);
    } else {
        // Invalid variable name, output original expression as-is
        fwrite(start, 1, expr_len, out);
        free(var_name);
        free(default_value);
    }
    
    *p = end;     // Move pointer to the closing brace
}

static void process_plain(FILE* out, char** p, struct envsubst_ctx* ctx) {
    char* start = *p;
    char* cur = start + 1;
    while (*cur && is_var_char((unsigned char)*cur)) cur++;

    size_t var_len = cur - start - 1;  // Exclude the '$' prefix
    
    if (var_len > 0) {
        // Validate: first char must be letter or underscore
        bool valid = true;
        if (!isalpha((unsigned char)start[1]) && start[1] != '_') {
            valid = false;
        }
        
        if (valid) {
            // Create a null-terminated copy of the variable name
            char* var = malloc(var_len + 1);
            if (var) {
                memcpy(var, start + 1, var_len);
                var[var_len] = 0;
                
                // Create original expression string for debug/stats
                size_t expr_len = var_len + 1;  // $ + var_name
                char* orig_expr = malloc(expr_len + 1);
                if (orig_expr) {
                    memcpy(orig_expr, start, expr_len);
                    orig_expr[expr_len] = 0;
                    output(out, ctx, var, orig_expr);
                    free(orig_expr);
                } else {
                    output(out, ctx, var, start);  // Fallback
                }
                free(var);
            } else {
                // Memory allocation failed, output original
                fputc('$', out);
            }
        } else {
            // Invalid variable name (starts with digit), output as-is
            fwrite(start, 1, var_len + 1, out);  // Include the '$'
        }
        *p = cur - 1;
    } else {
        // No valid variable name after $, just output $
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

    if (ferror(in)) {
        fprintf(stderr, "Error: Failed to read input stream\n");
        exit(EXIT_FAILURE);
    }
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
        {"debug",   no_argument,       NULL, 129},
        {"stats",   no_argument,       NULL, 130},
        {"json-stats", no_argument,    NULL, 131},
        {"whitelist-file", required_argument, NULL, 132},
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
            case 129: ctx.debug_mode = true; break;
            case 130: ctx.stats_mode = true; break;
            case 131: ctx.json_stats = true; break;
            case 132: 
                if (load_whitelist_file(&ctx, optarg) < 0) {
                    exit(EXIT_FAILURE);
                }
                break;
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
    print_stats(&ctx);  // 输出统计信息
    ctx_free(&ctx);
    return EXIT_SUCCESS;
}
