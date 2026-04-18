# `envsubst` - 生产级安全环境变量替换工具
安全高效、支持通配符白名单、专为 Nginx / Docker / Kubernetes 容器场景优化的增强版 `envsubst` 工具。

---

## 🌟 核心特性
- **Nginx 安全默认**：仅替换 `${VAR}` 格式，不破坏 `$host`/`$uri` 等内置变量
- **双模式切换**：`--all` 启用传统模式，同时替换 `$VAR` 和 `${VAR}`
- **强大通配符白名单**：支持前缀 `REST_*`、后缀 `*_PROD`、中间匹配 `APP_*_API`
- **安全兜底**：`-k` 保留未定义变量，不删除、不空白
- **零依赖、体积小**：纯 C 编写，适合容器镜像、嵌入式、生产环境
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
envsubst 1.0.0-tekin - 安全型环境变量替换工具 (支持通配符白名单)

【选项】
  -h, --help              显示帮助信息
  -V, --version           显示版本号
  -v, --variables         列出允许的匹配规则并退出
  -k, --keep-undefined    变量未定义时保留原字符串，不删除
      --all               全替换模式：同时替换 $VAR 和 ${VAR}
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

### 3. 启用全模式替换（兼容原生 envsubst）
```bash
envsubst --all < app.tpl > app.conf
```

### 4. 保留未定义变量（启动脚本必备）
```bash
envsubst -k 'APP_*' < config.tpl > config.conf
```

### 5. 管道模式
```bash
cat app.tpl | envsubst 'DB_* REDIS_*' > app.cfg
```

---

## 🐳 Docker 容器最佳实践

### Dockerfile 内置
```dockerfile
COPY envsubst /usr/local/bin/
COPY nginx.conf.template /etc/nginx/

# 启动前替换变量
CMD envsubst 'REST_* WAF_*' < /etc/nginx/nginx.conf.template > /etc/nginx/nginx.conf \
    && nginx -g 'daemon off;'
```

### Kubernetes 启动脚本
```bash
#!/bin/sh
envsubst -k 'APP_* API_*' < /app/config.template > /app/config.yaml
exec /app/main
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
1. **优先使用白名单通配符**，避免意外替换 Nginx 内置变量
2. **容器场景始终加 `-k`**，防止变量缺失导致配置异常
3. **Nginx 配置不要使用 `--all`**，避免破坏 `$proxy_host` 等变量
4. 多环境（dev/staging/prod）统一使用模板 + 环境变量注入
5. 编译后直接放入基础镜像，不增加额外依赖

---

## 🛠 项目信息
- **Repo**: https://github.com/tekintian/envsubst/
- **Version**: 1.0.0-tekin

---

## 📞 支持信息
- 微信公众号：**技术与认知**
- 站点：https://ai.tekin.cn
- 邮箱：tekintian@gmail.com

---
