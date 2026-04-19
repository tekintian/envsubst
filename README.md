# `envsubst` v2.0.0 - 生产级安全环境变量替换工具

安全高效、支持通配符白名单、默认值语法、**原生就地编辑**，专为 Nginx / Docker / Kubernetes 容器场景优化的增强版 `envsubst` 工具。

---

## 🌟 核心特性

- **Nginx 安全默认**：仅替换 `${VAR}` 格式，不破坏 `$host`/`$uri` 等内置变量
- **双模式切换**：`--all` 启用传统模式，同时替换 `$VAR` 和 `${VAR}`
- **强大通配符白名单**：支持前缀 `REST_*`、后缀 `*_PROD`、中间匹配 `APP_*_API`
- **默认值支持**：`${VAR:-default}` bash 风格语法，未设置时使用默认值
- **🆕 原生就地编辑**：`-i` / `--in-place` 选项，类似 `sed -i`，安全原子操作
- **备份功能**：`-i.bak` 自动备份原文件，支持自定义后缀
- **安全兜底**：`-k` 保留未定义变量，不删除、不空白
- **调试与统计**：`--debug` 显示替换过程，`--stats`/`--json-stats` 提供统计信息
- **配置文件支持**：`--whitelist-file` 从文件读取白名单规则
- **零依赖、体积小**：纯 C 编写（~50KB），适合容器镜像、嵌入式、生产环境
- **容器友好**：兼容标准输入输出，一键替换配置模板

---

## 📦 编译安装
```bash
# 编译（Linux/macOS 通用）
gcc -O2 -Wall -Wextra -std=c99 -pedantic envsubst.c -o envsubst

# 安装到系统
chmod +x envsubst
mv envsubst /usr/local/bin/
```

---

## 📖 完整参数说明

```
envsubst v2.0.0 - 安全型环境变量替换工具 (支持通配符白名单)

【选项】
  -h, --help              显示帮助信息
  -V, --version           显示版本号
  -v, --variables         列出允许的匹配规则并退出
  -k, --keep-undefined    变量未定义时保留原字符串，不删除
  -i, --in-place[=SUFFIX] 就地编辑文件（可选备份后缀）🆕
      --all               全替换模式：同时替换 $VAR 和 ${VAR}
      --debug             调试模式：显示每个变量的替换过程
      --stats             统计模式：显示替换统计信息
      --json-stats        JSON 格式统计信息（便于机器解析）
      --whitelist-file F  从文件 F 读取白名单规则（每行一个）
```

---

## 🚀 常用使用示例

### 1. 标准替换（容器最常用）
```bash
envsubst < /etc/nginx/nginx.conf.template > /etc/nginx/nginx.conf
```

### 2. 白名单通配符（生产推荐，只替换指定前缀）
```bash
envsubst 'REST_* WAF_* CONFIG_*' < nginx.conf.template > nginx.conf
```

### 3. 默认值支持（未设置时使用默认值）
```bash
# 变量未设置，使用默认值
echo '${HOST:-localhost}' | envsubst
# 输出: localhost

# 变量已设置，使用环境变量
echo '${HOST:-localhost}' | HOST=example.com envsubst
# 输出: example.com

# Nginx 配置中的默认值
echo 'listen ${PORT:-80}; server_name ${HOST:-localhost};' | envsubst
# 输出: listen 80; server_name localhost;

# ⚠️ 注意：白名单会影响默认值的行为
echo '${ABCD:-9999999}' | envsubst           # 输出: 9999999 ✅
echo '${ABCD:-9999999}' | envsubst 'REST_*'  # 输出: ${ABCD:-9999999} (不在白名单，原样保留)
echo '${ABCD:-9999999}' | envsubst 'ABCD'    # 输出: 9999999 ✅ (在白名单)
```

### 4. 启用全模式替换（兼容原生 envsubst）
```bash
envsubst --all < app.tpl > app.conf
```

### 5. 保留未定义变量（启动脚本必备）
```bash
envsubst -k 'APP_*' < config.tpl > config.conf
```

### 6. 调试模式（查看替换过程）
```bash
echo '${HOST} ${PORT}' | HOST=localhost envsubst --debug 'HOST_*' 2>&1
# 输出:
# [DEBUG] Replace: ${HOST} -> localhost
# [DEBUG] Skip (not in whitelist): ${PORT}
# localhost ${PORT}
```

### 7. 统计信息（CI/CD 质量检查）
```bash
# 人类可读格式
echo '${A} ${B}' | A=1 envsubst --stats 2>&1
# 输出:
# === envsubst Statistics ===
# Variables replaced:    2
# Variables kept:        0
# Variables skipped:     0
# Total processed:       3
# ========================

# JSON 格式（便于机器解析）
echo '${A} ${B}' | A=1 envsubst --json-stats 2>stats.json
jq '.envsubst_stats.variables_replaced' stats.json
# 输出: 2
```

### 8. 🆕 就地编辑（v2.0.0+）
```bash
# 基本就地编辑
envsubst -i 'APP_* DB_*' config.conf

# 带备份的就地编辑
envsubst -i.bak 'APP_* DB_*' config.conf
# 结果:
#   config.conf      ← 新内容
#   config.conf.bak  ← 原内容备份

# 自定义备份后缀
envsubst -i.20250101 'PROD_*' prod.conf
# 结果: prod.conf.20250101

# 无白名单（全部替换）
envsubst -i config.conf
```

**优势**：
- ✅ 语法简洁优雅（类似 `sed -i`）
- ✅ 原子操作，安全可靠
- ✅ 自动清理临时文件
- ✅ 可选备份功能

详见 [docs/IN_PLACE_EDITING.md](docs/IN_PLACE_EDITING.md)

### 9. 配置文件白名单
```bash
# 创建白名单文件 rules.txt
cat > rules.txt << EOF
# 这是注释
REST_*
WAF_*
APP_*
EOF

# 使用白名单文件
envsubst --whitelist-file rules.txt < template.conf > output.conf
```

### 10. 管道模式
```bash
cat app.tpl | envsubst 'DB_* REDIS_*' > app.cfg
```

---

## 🐳 Docker 容器最佳实践

### Dockerfile 内置

**传统方式（v1.x）**：
```dockerfile
COPY envsubst /usr/local/bin/
COPY nginx.conf.template /etc/nginx/

# 启动前替换变量
CMD envsubst 'REST_* WAF_*' < /etc/nginx/nginx.conf.template > /etc/nginx/nginx.conf \
    && nginx -g 'daemon off;'
```

**🆕 推荐方式（v2.0.0+）**：
```dockerfile
COPY envsubst /usr/local/bin/
COPY nginx.conf.template /etc/nginx/

# 使用原地编辑，更简洁安全
CMD envsubst -i.bak 'REST_* WAF_*' /etc/nginx/nginx.conf.template \
    && mv /etc/nginx/nginx.conf.template /etc/nginx/nginx.conf \
    && nginx -g 'daemon off;'
```

### 多阶段构建（推荐）
```dockerfile
# 第一阶段：编译 envsubst
FROM gcc:alpine AS builder
WORKDIR /build
COPY envsubst.c .
RUN gcc -O2 -Wall -Wextra -std=c99 envsubst.c -o envsubst

# 第二阶段：生产镜像
FROM nginx:alpine
COPY --from=builder /build/envsubst /usr/local/bin/
COPY nginx.conf.template /etc/nginx/
COPY docker-entrypoint.sh /docker-entrypoint.sh
RUN chmod +x /docker-entrypoint.sh

ENTRYPOINT ["/docker-entrypoint.sh"]
CMD ["nginx", "-g", "daemon off;"]
```

`docker-entrypoint.sh`:
```bash
#!/bin/sh
set -e

echo "🔧 Generating nginx configuration..."

# 使用白名单安全替换，保护 Nginx 内置变量
envsubst 'NGINX_* WAF_* APP_*' \
    < /etc/nginx/nginx.conf.template \
    > /etc/nginx/nginx.conf

echo "✅ Configuration generated successfully"
exec "$@"
```

### 🆕 使用预构建镜像（最简单）

从 **v3.5** 开始，以下 Alpine 镜像已内置最新版本的 envsubst v2.0.0+：

#### 可用镜像源

| 镜像源 | 镜像名称 | 版本支持 |
|--------|---------|----------|
| **GitHub Container Registry** | `ghcr.io/tekintian/alpine` | 3.5 - 3.23 ✅ |
| **Docker Hub** | `tekintian/alpine` | 3.5 - 3.23 ✅ |
| **阿里云容器镜像** | `registry.cn-hangzhou.aliyuncs.com/alpine-docker/alpine` | 3.5 - 3.23 ✅ |

**查看所有版本**: https://github.com/tekintian/alpine/pkgs/container/alpine

#### 示例 1: 直接使用镜像

```dockerfile
# 使用 GitHub Container Registry (推荐)
FROM ghcr.io/tekintian/alpine:3.21

# envsubst 已经内置，直接使用！
COPY nginx.conf.template /etc/nginx/

CMD envsubst -i.bak 'NGINX_* APP_*' /etc/nginx/nginx.conf.template \
    && mv /etc/nginx/nginx.conf.template /etc/nginx/nginx.conf \
    && nginx -g 'daemon off;'
```

#### 示例 2: 国内加速（阿里云）

```dockerfile
# 使用阿里云镜像（国内访问更快）
FROM registry.cn-hangzhou.aliyuncs.com/alpine-docker/alpine:3.21

COPY nginx.conf.template /etc/nginx/

# 直接使用内置的 envsubst
RUN envsubst -i 'NGINX_*' /etc/nginx/nginx.conf.template

CMD ["nginx", "-g", "daemon off;"]
```

#### 示例 3: Docker Compose 中使用

```yaml
version: '3.8'
services:
  nginx:
    image: ghcr.io/tekintian/alpine:3.21
    volumes:
      - ./nginx.conf.template:/etc/nginx/nginx.conf.template
      - ./html:/usr/share/nginx/html
    environment:
      - NGINX_HOST=example.com
      - NGINX_PORT=80
      - APP_BACKEND=http://api:8080
    ports:
      - "80:80"
    command: >
      sh -c "envsubst -i.bak 'NGINX_* APP_*' /etc/nginx/nginx.conf.template
             && mv /etc/nginx/nginx.conf.template /etc/nginx/nginx.conf
             && nginx -g 'daemon off;'"
```

#### 示例 4: Kubernetes ConfigMap + InitContainer

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: nginx-app
spec:
  template:
    spec:
      # 使用 init container 生成配置
      initContainers:
      - name: config-generator
        image: ghcr.io/tekintian/alpine:3.21
        command: ['sh', '-c']
        args:
        - |
          echo "Generating nginx config..."
          envsubst -i 'NGINX_* APP_* DB_*' /config/nginx.conf.template
          echo "✅ Config generated"
        volumeMounts:
        - name: config-volume
          mountPath: /config
        envFrom:
        - configMapRef:
            name: nginx-env-vars
      
      # 主容器
      containers:
      - name: nginx
        image: nginx:alpine
        ports:
        - containerPort: 80
        volumeMounts:
        - name: config-volume
          mountPath: /etc/nginx/conf.d
      
      volumes:
      - name: config-volume
        emptyDir: {}
```

#### 优势对比

| 方式 | 优点 | 缺点 |
|------|------|------|
| **预构建镜像** | ✅ 零配置<br>✅ 即时可用<br>✅ 自动更新 | ⚠️ 依赖外部镜像 |
| 多阶段构建 | ✅ 完全可控<br>✅ 无外部依赖 | ⚠️ 构建时间长<br>⚠️ 需要维护 |
| 手动复制二进制 | ✅ 简单直接 | ⚠️ 需要编译<br>⚠️ 平台兼容性问题 |

**推荐场景**：
- 🚀 **快速原型/开发环境**：使用预构建镜像
- 🏭 **生产环境**：根据团队策略选择（预构建或多阶段构建）
- 🌏 **国内用户**：优先使用阿里云镜像

### Kubernetes 启动脚本
```bash
#!/bin/sh
envsubst -k 'APP_* API_*' < /app/config.template > /app/config.yaml
exec /app/main
```

### 使用默认值的 Docker 配置
```dockerfile
FROM nginx:alpine
COPY envsubst /usr/local/bin/
COPY config.tpl /etc/nginx/

# 不提供环境变量时使用默认值
CMD envsubst < /etc/nginx/config.tpl > /etc/nginx/nginx.conf \
    && nginx -g 'daemon off;'
```

```bash
# 开发环境 - 使用默认值
docker run myapp

# 生产环境 - 覆盖关键配置
docker run -e PORT=443 -e HOST=prod.example.com myapp
```

### Docker 一次性运行
```bash
docker run --rm \
  -e REST_HOST=api.example.com \
  -v $(pwd)/app.tpl:/tmp/app.tpl \
  your-image envsubst < /tmp/app.tpl > /tmp/app.out
```

---

## ✅ 最佳实践

### 安全性
1. **优先使用白名单通配符**，避免意外替换 Nginx 内置变量
2. **Nginx 配置不要使用 `--all`**，避免破坏 `$proxy_host` 等变量
3. **容器场景始终加 `-k`**，防止变量缺失导致配置异常

### 可维护性
4. **使用默认值**：为非关键配置提供合理的默认值（`${VAR:-default}`）
5. **命名规范**：使用前缀区分不同模块（`DB_*`, `API_*`, `CACHE_*`）
6. **模板注释**：在模板中标注哪些变量需要替换
7. **版本控制**：将 `.template` 文件纳入版本控制，生成的配置文件忽略

### CI/CD 集成
8. **使用 JSON 统计**：在 CI/CD 中使用 `--json-stats` 进行质量检查
9. **白名单文件**：将白名单规则纳入版本控制，便于审查
10. **多环境配置**：dev/staging/prod 统一使用模板 + 环境变量注入

### 性能
11. **精简白名单**：只列出实际需要的变量前缀
12. **预编译二进制**：在 Docker 镜像中直接使用编译好的二进制
13. **流式处理**：对于大文件，使用管道而非临时文件

---

## 🔧 故障排查

### 变量未被替换

**问题**: 模板中的 `${VAR}` 没有被替换成实际值

**解决方案**:
1. 确认变量已导出为环境变量：
   ```bash
   export MY_VAR=value  # ✅ 正确
   MY_VAR=value         # ❌ 错误（只是 shell 变量）
   ```

2. 检查变量名格式：
   ```bash
   # ✅ 合法的变量名
   ${MY_VAR}
   ${APP_HOST_123}
   
   # ❌ 非法的变量名（会原样保留）
   ${MY VAR}      # 包含空格
   ${123NUM}      # 以数字开头
   ${VAR@HOST}    # 包含特殊字符
   ```

3. 使用 `-v` 查看当前允许的规则：
   ```bash
   envsubst -v 'REST_* WAF_*'
   # 输出:
   # REST_*
   # WAF_*
   ```

4. 使用 `--debug` 调试：
   ```bash
   echo '${MY_VAR}' | MY_VAR=value envsubst --debug 2>&1
   # 输出: [DEBUG] Replace: ${MY_VAR} -> value
   ```

### Nginx 配置损坏

**问题**: 替换后 Nginx 启动失败，报错未知变量

**原因**: 使用了 `--all` 模式，导致 Nginx 内置变量（如 `$host`, `$uri`）被误替换

**解决方案**:
```bash
# ❌ 错误做法
envsubst --all < nginx.conf.template > nginx.conf

# ✅ 正确做法 - 使用白名单
envsubst 'REST_* WAF_* APP_*' < nginx.conf.template > nginx.conf
```

### 通配符不生效

**问题**: 设置了白名单但变量仍未被替换

**检查清单**:
1. 通配符语法是否正确：
   ```bash
   # ✅ 支持的格式
   'PREFIX_*'        # 前缀匹配
   '*_SUFFIX'        # 后缀匹配
   'MID_*_MID'       # 中间匹配
   
   # ❌ 不支持的格式
   'VAR'             # 缺少通配符 *
   '*VAR*'           # 多个通配符（只支持一个 *）
   ```

2. 多个规则是否用空格或逗号分隔：
   ```bash
   # ✅ 正确
   envsubst 'A_* B_* C_*'
   envsubst 'A_*,B_*,C_*'
   
   # ❌ 错误
   envsubst 'A_*' 'B_*'  # 会被当作多个参数
   ```

3. 验证规则解析：
   ```bash
   envsubst -v 'REST_* WAF_*'
   ```

### 默认值不生效

**检查**:
```bash
# 确保语法正确
echo '${VAR:-default}' | envsubst --debug 2>&1

# 常见错误:
# ${VAR-default}     ❌ 缺少冒号
# ${VAR: -default}   ❌ 有空格
# ${VAR:-default}    ✅ 正确
```

**白名单影响**:
```bash
# 如果使用了白名单，变量必须在白名单中才能替换
echo '${ABCD:-9999999}' | envsubst           # ✅ 输出: 9999999
echo '${ABCD:-9999999}' | envsubst 'REST_*'  # ⚠️ 输出: ${ABCD:-9999999} (不在白名单)
echo '${ABCD:-9999999}' | envsubst 'ABCD'    # ✅ 输出: 9999999 (在白名单)

# 解决方案：将变量加入白名单或使用通配符
envsubst 'ABCD REST_* WAF_*' < template.conf
envsubst '*' < template.conf  # 匹配所有变量
```

### JSON 统计解析失败

**检查**:
```bash
# 确保重定向 stderr
envsubst --json-stats < tpl.conf 2>stats.json

# 验证 JSON 格式
jq '.' stats.json

# 常见错误:
# envsubst --json-stats < tpl.conf > stats.json  ❌ 错误，stdout 是配置内容
# envsubst --json-stats < tpl.conf 2>stats.json  ✅ 正确，stderr 是 JSON
```

### 🆕 同文件读写问题（v2.0.0+ 已解决）

**问题**: `envsubst < file > file` 会清空文件

**原因**: Shell 重定向机制导致，不是 envsubst 的 bug

**解决方案**:

1. **🆕 推荐：使用 `-i` 选项（v2.0.0+）**
   ```bash
   envsubst -i 'APP_*' config.conf
   envsubst -i.bak 'APP_*' config.conf  # 带备份
   ```

2. **传统方式：使用临时文件**
   ```bash
   envsubst < config.tpl > config.tmp && mv config.tmp config.tpl
   ```

3. **使用 sponge 工具**
   ```bash
   envsubst < config.tpl | sponge config.tpl
   ```

详见 [docs/SPECIAL_SCENARIOS.md](docs/SPECIAL_SCENARIOS.md)

---

## 📊 与原生 envsubst 对比

| 特性 | GNU envsubst | 本工具 |
|------|--------------|--------|
| `${VAR}` 替换 | ✅ | ✅ |
| `$VAR` 替换 | ✅ | ⚠️ 需 `--all` |
| 保护 Nginx 变量 | ❌ | ✅ 默认启用 |
| 通配符白名单 | ❌ | ✅ 支持 |
| 保留未定义变量 | ❌ | ✅ `-k` 选项 |
| 默认值支持 | ❌ | ✅ `${VAR:-default}` |
| 非法变量名处理 | 删除/空白 | ✅ 原样保留 |
| 调试模式 | ❌ | ✅ `--debug` |
| 统计信息 | ❌ | ✅ `--stats`/`--json-stats` |
| 配置文件支持 | ❌ | ✅ `--whitelist-file` |
| 🆕 **就地编辑** | ❌ | ✅ **`-i` / `--in-place`** |
| 二进制大小 | ~17KB | ~50KB |
| 依赖库 | gettext (libintl) | 无（零依赖） |
| 容器友好度 | 一般 | ✅ 优秀 |
| 适用场景 | 通用 | Nginx/Docker/K8s |

**选择建议**:
- 🟢 **Nginx 配置**: 必须使用本工具（保护内置变量）
- 🟢 **Docker/K8s**: 推荐本工具（零依赖、白名单更安全）
- 🟡 **通用场景**: 两者均可，本工具提供更多安全特性
- 🔴 **需要 `$VAR` 频繁替换**: 考虑使用 `--all` 或其他工具

---

## 🛠 项目信息
- **Repo**: https://github.com/tekintian/envsubst/
- **Version**: v2.0.0
- **License**: MIT

### 📚 相关文档
- [IN_PLACE_EDITING.md](docs/IN_PLACE_EDITING.md) - 就地编辑完整指南 🆕
- [SPECIAL_SCENARIOS.md](docs/SPECIAL_SCENARIOS.md) - 特殊场景分析
- [CSDN_ARTICLE.md](CSDN_ARTICLE.md) - 技术文章

---

## 📞 支持信息
- 微信公众号：**技术与认知**
- 站点：https://ai.tekin.cn
- 邮箱：tekintian@gmail.com
