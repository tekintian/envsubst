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
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// 检测是否在容器环境中运行
static bool is_container_environment(void) {
    // 方法 1: 检查 /.dockerenv 文件（Docker）
    if (access("/.dockerenv", F_OK) == 0) {
        return true;
    }
    
    // 方法 2: 检查 /run/.containerenv 文件（Podman）
    if (access("/run/.containerenv", F_OK) == 0) {
        return true;
    }
    
    // 方法 3: 检查 cgroup 中是否包含 docker/containerd/kubepods
    FILE* cgroup = fopen("/proc/1/cgroup", "r");
    if (cgroup) {
        char line[256];
        while (fgets(line, sizeof(line), cgroup)) {
            if (strstr(line, "docker") || 
                strstr(line, "containerd") || 
                strstr(line, "kubepods") ||
                strstr(line, "lxc")) {
                fclose(cgroup);
                return true;
            }
        }
        fclose(cgroup);
    }
    
    // 方法 4: 检查环境变量
    const char* container_env = getenv("CONTAINER");
    if (container_env && strlen(container_env) > 0) {
        return true;
    }
    
    const char* kubernetes_env = getenv("KUBERNETES_SERVICE_HOST");
    if (kubernetes_env && strlen(kubernetes_env) > 0) {
        return true;
    }
    
    return false;
}

// 检测文件是否在挂载卷上
static bool is_mounted_volume(const char* filename) {
    struct stat st_file;
    struct stat st_parent;
    char parent_dir[PATH_MAX];
    
    // 获取父目录
    strncpy(parent_dir, filename, sizeof(parent_dir) - 1);
    parent_dir[sizeof(parent_dir) - 1] = '\0';
    
    char* last_slash = strrchr(parent_dir, '/');
    if (last_slash && last_slash != parent_dir) {
        *last_slash = '\0';
    } else {
        strcpy(parent_dir, "/");
    }
    
    // 获取文件和父目录的 stat 信息
    if (stat(filename, &st_file) != 0) {
        return false;  // 文件不存在，无法判断
    }
    
    if (stat(parent_dir, &st_parent) != 0) {
        return false;  // 父目录无法访问
    }
    
    // 关键检测：比较设备和 inode
    // 如果文件的设备ID与父目录不同，说明是挂载点
    if (st_file.st_dev != st_parent.st_dev) {
        return true;
    }
    
    // 额外检测：检查是否是 overlay/aufs 文件系统（Docker 常见）
    FILE* mounts = fopen("/proc/mounts", "r");
    if (mounts) {
        char line[512];
        while (fgets(line, sizeof(line), mounts)) {
            // 检查是否包含 overlay, aufs, bind 等挂载类型
            if (strstr(line, "overlay") || 
                strstr(line, "aufs") ||
                (strstr(line, "bind") && strstr(line, parent_dir))) {
                // 进一步检查这个挂载点是否影响我们的文件
                char mount_point[256];
                if (sscanf(line, "%*s %255s", mount_point) == 1) {
                    if (strncmp(filename, mount_point, strlen(mount_point)) == 0) {
                        fclose(mounts);
                        return true;
                    }
                }
            }
        }
        fclose(mounts);
    }
    
    return false;
}

#ifndef ENVSUBST_VERSION
#define ENVSUBST_VERSION "v2.2.0"
#endif

struct envsubst_ctx {
    char** allowed_vars;
    size_t var_count;
    bool keep_undefined;
    bool replace_all;
    bool debug_mode;      // 调试模式
    bool stats_mode;      // 统计模式
    bool json_stats;      // JSON 格式统计
    bool in_place;        // 就地编辑模式
    bool safe_mode;       // 安全模式（使用内容覆盖而非 rename）
    char* in_place_backup; // 就地编辑备份后缀
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
    printf("  支持默认值语法：${VAR:-default}\n");
    printf("  支持条件替换：${VAR:+value} (变量存在时输出 value，不支持嵌套变量)\n");
    printf("\n");
    printf("【使用方法】\n");
    printf("  envsubst [选项] [变量名/通配符...]\n");
    printf("  envsubst -i [选项] [变量名/通配符...] FILE  # 就地编辑\n");
    printf("\n");
    printf("【选项】\n");
    printf("  -h, --help              显示本帮助信息并退出\n");
    printf("  -V, --version           显示版本信息并退出\n");
    printf("  -v, --variables         列出允许替换的变量/通配符并退出\n");
    printf("  -k, --keep-undefined    变量未定义时保留原字符串，不删除\n");
    printf("  -i, --in-place[=SUFFIX] 就地编辑文件（可选备份后缀）\n");
    printf("  -s, --safe              安全模式：使用内容覆盖 (自动检测挂载卷)\n");
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
    ctx->in_place = false;
    ctx->safe_mode = false;
    ctx->in_place_backup = NULL;
    ctx->replaced_count = 0;
    ctx->kept_count = 0;
    ctx->skipped_count = 0;
}

static void ctx_free(struct envsubst_ctx* ctx) {
    for (size_t i = 0; i < ctx->var_count; i++)
        free(ctx->allowed_vars[i]);
    free(ctx->allowed_vars);
    if (ctx->in_place_backup) {
        free(ctx->in_place_backup);
    }
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

// Forward declaration for recursive processing
static void process_stream(FILE* in, FILE* out, struct envsubst_ctx* ctx);

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
    
    // Check for default value or conditional syntax ${VAR:-default} or ${VAR:+value}
    char* var_name = NULL;
    char* modifier_value = NULL;  // Can be default value or conditional value
    char modifier_type = 0;       // '-' for :-, '+' for :+
    size_t var_len = 0;
    
    // Look for :- or :+ pattern
    char* modifier_pos = NULL;
    for (size_t i = 0; i < content_len - 1; i++) {
        if (content[i] == ':' && (content[i+1] == '-' || content[i+1] == '+')) {
            modifier_pos = (char*)content + i;
            modifier_type = content[i+1];
            break;
        }
    }
    
    if (modifier_pos) {
        // Has modifier (:- or :+)
        var_len = modifier_pos - content;
        size_t value_len = content_len - var_len - 2;  // -2 for :- or :+
        
        var_name = malloc(var_len + 1);
        modifier_value = malloc(value_len + 1);
        
        if (var_name && modifier_value) {
            memcpy(var_name, content, var_len);
            var_name[var_len] = 0;
            memcpy(modifier_value, modifier_pos + 2, value_len);
            modifier_value[value_len] = 0;
        } else {
            free(var_name);
            free(modifier_value);
            var_name = NULL;
            modifier_value = NULL;
        }
    } else {
        // No modifier
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
            free(modifier_value);
            *p = end;
            return;
        }
        
        // Get environment variable value
        const char* env_val = getenv(var_name);
        const char* final_value = NULL;
        bool should_output = true;
        
        // Handle modifiers
        if (modifier_type == '-') {
            // ${VAR:-default} - use default if not set or empty
            if (!env_val || env_val[0] == '\0') {
                final_value = modifier_value;
                if (ctx->debug_mode) {
                    fprintf(stderr, "[DEBUG] Use default: ${%s:-%s} -> %s\n", 
                            var_name, modifier_value, modifier_value);
                }
            } else {
                final_value = env_val;
                if (ctx->debug_mode) {
                    fprintf(stderr, "[DEBUG] Replace: ${%s:-...} -> %s\n", var_name, final_value);
                }
            }
        } else if (modifier_type == '+') {
            // ${VAR:+value} - use value only if VAR is set and non-empty
            if (env_val && env_val[0] != '\0') {
                // For :+ modifier with nested ${...}, we need to resolve them
                // Check if there are nested variables
                if (strstr(modifier_value, "${")) {
                    // Has nested variables - for now, output as-is with a note
                    // Full recursive processing would require significant refactoring
                    final_value = modifier_value;
                    if (ctx->debug_mode) {
                        fprintf(stderr, "[DEBUG] Conditional (nested vars not resolved): ${%s:+%s}\n", 
                                var_name, modifier_value);
                    }
                } else {
                    // No nested variables, simple case
                    final_value = modifier_value;
                    if (ctx->debug_mode) {
                        fprintf(stderr, "[DEBUG] Conditional: ${%s:+%s} -> %s\n", 
                                var_name, modifier_value, modifier_value);
                    }
                }
            } else {
                // Variable not set or empty, output nothing
                final_value = "";
                should_output = false;
                if (ctx->debug_mode) {
                    fprintf(stderr, "[DEBUG] Conditional skip: ${%s:+...} (not set)\n", var_name);
                }
            }
        } else {
            // No modifier, simple replacement
            final_value = env_val;
            if (final_value && ctx->debug_mode) {
                fprintf(stderr, "[DEBUG] Replace: ${%s} -> %s\n", var_name, final_value);
            }
        }
        
        if (final_value && should_output) {
            ctx->replaced_count++;
            fputs(final_value, out);
        } else if (!should_output) {
            // Conditional replacement with empty result
            // Don't count as replaced, just skip
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
        free(modifier_value);
    } else {
        // Invalid variable name, output original expression as-is
        fwrite(start, 1, expr_len, out);
        free(var_name);
        free(modifier_value);
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

// 就地编辑处理函数
static void process_in_place(const char* filename, struct envsubst_ctx* ctx) {
    // 智能检测：是否在容器环境且文件在挂载卷上
    bool in_container = is_container_environment();
    bool on_mounted_vol = in_container ? is_mounted_volume(filename) : false;
    
    // 自动选择模式：
    // - 挂载卷：强制使用安全模式（即使用户未指定 -s）
    // - 非挂载卷：使用用户指定的模式
    bool use_safe_mode = ctx->safe_mode || on_mounted_vol;
    
    if (on_mounted_vol && !ctx->safe_mode) {
        // 检测到挂载卷但未指定 -s，自动启用并提示
        fprintf(stderr, "Info: Mounted volume detected, using safe mode automatically\n");
    }
    
    // 智能备份逻辑：
    // - 容器环境：默认不备份（除非明确指定 -i.SUFFIX）
    // - 非容器环境：如果指定了 -i.SUFFIX 则备份
    bool should_backup = ctx->in_place_backup != NULL;
    
    if (in_container && !ctx->in_place_backup) {
        // 容器环境且未明确指定备份，静默跳过
        should_backup = false;
    } else if (in_container && ctx->in_place_backup) {
        // 容器环境但用户明确要求备份，执行备份并提示
        fprintf(stderr, "Info: Creating backup in container environment (explicitly requested)\n");
    }
    
    // 创建临时文件
    char tmpfile[PATH_MAX];
    snprintf(tmpfile, sizeof(tmpfile), "%s.XXXXXX", filename);
    
    int fd = mkstemp(tmpfile);
    if (fd == -1) {
        fprintf(stderr, "Error: Cannot create temporary file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    FILE* tmpfp = fdopen(fd, "w");
    if (!tmpfp) {
        close(fd);
        unlink(tmpfile);
        fprintf(stderr, "Error: Cannot open temporary file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // 打开输入文件
    FILE* infile = fopen(filename, "r");
    if (!infile) {
        fclose(tmpfp);
        unlink(tmpfile);
        fprintf(stderr, "Error: Cannot open input file '%s': %s\n", 
                filename, strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // 执行替换
    process_stream(infile, tmpfp, ctx);
    
    // 关闭文件
    fclose(infile);
    if (fclose(tmpfp) != 0) {
        unlink(tmpfile);
        fprintf(stderr, "Error: Failed to write temporary file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // 替换文件
    if (use_safe_mode) {
        // 安全模式：使用内容覆盖（兼容 Docker 挂载卷）
        
        // 保存原文件的权限和所有者信息
        struct stat orig_stat;
        bool has_orig_stat = (stat(filename, &orig_stat) == 0);
        
        // 备份原文件（如果需要）
        if (should_backup) {
            char backup[PATH_MAX];
            snprintf(backup, sizeof(backup), "%s%s", filename, ctx->in_place_backup);
            
            FILE* src = fopen(filename, "r");
            if (!src) {
                unlink(tmpfile);
                fprintf(stderr, "Error: Cannot read file for backup: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            
            FILE* dst = fopen(backup, "w");
            if (!dst) {
                fclose(src);
                unlink(tmpfile);
                fprintf(stderr, "Error: Cannot create backup '%s': %s\n", 
                        backup, strerror(errno));
                exit(EXIT_FAILURE);
            }
            
            char buffer[8192];
            size_t bytes;
            while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
                fwrite(buffer, 1, bytes, dst);
            }
            
            fclose(src);
            fclose(dst);
        }
        
        // 1. 打开临时文件读取
        FILE* tmp_read = fopen(tmpfile, "r");
        if (!tmp_read) {
            unlink(tmpfile);
            fprintf(stderr, "Error: Cannot read temporary file: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        // 2. 打开原文件写入（截断）
        FILE* orig_write = fopen(filename, "w");
        if (!orig_write) {
            fclose(tmp_read);
            unlink(tmpfile);
            fprintf(stderr, "Error: Cannot write to file '%s': %s\n", 
                    filename, strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        // 3. 复制内容
        char buffer[8192];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), tmp_read)) > 0) {
            if (fwrite(buffer, 1, bytes, orig_write) != bytes) {
                fclose(tmp_read);
                fclose(orig_write);
                unlink(tmpfile);
                fprintf(stderr, "Error: Failed to write content\n");
                exit(EXIT_FAILURE);
            }
        }
        
        // 4. 关闭文件
        fclose(tmp_read);
        fclose(orig_write);
        
        // 5. 恢复原文件的权限和所有者
        if (has_orig_stat) {
            // 恢复权限模式
            if (chmod(filename, orig_stat.st_mode) != 0) {
                fprintf(stderr, "Warning: Cannot restore permissions: %s\n", strerror(errno));
            }
            
            // 恢复所有者（需要 root 权限）
            if (chown(filename, orig_stat.st_uid, orig_stat.st_gid) != 0) {
                // 非 root 用户可能失败，只警告不退出
                if (errno != EPERM) {
                    fprintf(stderr, "Warning: Cannot restore ownership: %s\n", strerror(errno));
                }
            }
        }
        
        // 6. 删除临时文件
        unlink(tmpfile);
    } else {
        // 标准模式：使用 rename (原子操作)
        
        // 备份原文件（如果需要）
        if (should_backup) {
            char backup[PATH_MAX];
            snprintf(backup, sizeof(backup), "%s%s", filename, ctx->in_place_backup);
            if (link(filename, backup) != 0 && errno != EEXIST) {
                unlink(tmpfile);
                fprintf(stderr, "Error: Cannot create backup '%s': %s\n", 
                        backup, strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
        
        // 原子替换
        if (rename(tmpfile, filename) != 0) {
            unlink(tmpfile);
            fprintf(stderr, "Error: Cannot replace file '%s': %s\n", 
                    filename, strerror(errno));
            fprintf(stderr, "Hint: For Docker volumes, use -s/--safe option\n");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv) {
    struct envsubst_ctx ctx;
    ctx_init(&ctx);

    static const struct option opts[] = {
        {"help",    no_argument,       NULL, 'h'},
        {"version", no_argument,       NULL, 'V'},
        {"variables",no_argument,      NULL, 'v'},
        {"keep-undefined",no_argument,  NULL, 'k'},
        {"in-place", optional_argument, NULL, 'i'},
        {"safe",    no_argument,       NULL, 's'},
        {"all",     no_argument,       NULL, 128},
        {"debug",   no_argument,       NULL, 129},
        {"stats",   no_argument,       NULL, 130},
        {"json-stats", no_argument,    NULL, 131},
        {"whitelist-file", required_argument, NULL, 132},
        {0, 0, 0, 0}
    };

    int opt;
    bool list_vars = false;
    while ((opt = getopt_long(argc, argv, "hvVk::i::s", opts, NULL)) != -1) {
        switch (opt) {
            case 'h': usage(); break;
            case 'V': version(); break;
            case 'v': list_vars = true; break;
            case 'k': ctx.keep_undefined = true; break;
            case 'i':
                ctx.in_place = true;
                // optional_argument: optarg 为 NULL 表示没有参数
                if (optarg) {
                    ctx.in_place_backup = strdup(optarg);
                }
                break;
            case 's':
                ctx.safe_mode = true;
                break;
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

    // 就地编辑模式
    if (ctx.in_place) {
        // 需要指定文件名
        if (optind >= argc) {
            fprintf(stderr, "Error: In-place editing requires a filename\n");
            fprintf(stderr, "Usage: envsubst -i [OPTIONS] [PATTERNS...] FILE\n");
            ctx_free(&ctx);
            exit(EXIT_FAILURE);
        }
        
        const char* filename = argv[argc - 1];
        
        // 如果最后一个参数是文件，前面的都是白名单规则
        struct stat st;
        if (stat(filename, &st) == 0 && S_ISREG(st.st_mode)) {
            // 解析白名单（排除最后一个参数）
            ctx.var_count = 0;  // 重置
            free(ctx.allowed_vars);
            ctx.allowed_vars = NULL;
            for (int i = optind; i < argc - 1; i++)
                parse_arg(&ctx, argv[i]);
            
            process_in_place(filename, &ctx);
            print_stats(&ctx);
        } else {
            fprintf(stderr, "Error: '%s' is not a regular file\n", filename);
            ctx_free(&ctx);
            exit(EXIT_FAILURE);
        }
    } else {
        // 标准流模式
        process_stream(stdin, stdout, &ctx);
        print_stats(&ctx);
    }
    
    ctx_free(&ctx);
    return EXIT_SUCCESS;
}
