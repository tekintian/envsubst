# envsubst 高级功能使用指南

## 🎯 新增高级功能

本次更新添加了三个强大的高级功能：

1. ✅ **默认值支持** (`${VAR:-default}`) - bash 风格的默认值语法
2. ✅ **配置文件支持** (`--whitelist-file`) - 从文件读取白名单规则
3. ✅ **JSON 输出格式** (`--json-stats`) - 便于机器解析的统计信息

---

## 1️⃣ 默认值支持 `${VAR:-default}`

### 基本语法

```bash
${VARIABLE_NAME:-default_value}
```

当环境变量未设置或为空时，使用默认值。

### 使用示例

#### 示例 1: 基本默认值

```bash
# 变量未设置，使用默认值
echo '${HOST:-localhost}' | ./envsubst
# 输出: localhost

# 变量已设置，使用环境变量
echo '${HOST:-localhost}' | HOST=example.com ./envsubst
# 输出: example.com
```

#### 示例 2: Nginx 配置中的默认值

```bash
cat > nginx.conf.tpl << 'TPL'
server {
    listen ${PORT:-80};
    server_name ${HOST:-localhost};
    
    location / {
        proxy_pass http://${BACKEND:-127.0.0.1:3000};
    }
}
TPL

# 不提供任何环境变量，全部使用默认值
./envsubst < nginx.conf.tpl

# 输出:
# server {
#     listen 80;
#     server_name localhost;
#     
#     location / {
#         proxy_pass http://127.0.0.1:3000;
#     }
# }
```

#### 示例 3: 部分覆盖默认值

```bash
echo 'Database: ${DB_HOST:-localhost}:${DB_PORT:-5432}/${DB_NAME:-myapp}' | \
    DB_HOST=prod-db.example.com ./envsubst

# 输出: Database: prod-db.example.com:5432/myapp
# DB_PORT 和 DB_NAME 使用默认值
```

#### 示例 4: 复杂默认值

```bash
# 默认值可以包含特殊字符
echo '${URL:-http://localhost:8080/api/v1}' | ./envsubst
# 输出: http://localhost:8080/api/v1

# 默认值可以是路径
echo '${CONFIG_PATH:-/etc/app/config.yaml}' | ./envsubst
# 输出: /etc/app/config.yaml
```

### 与调试模式结合

```bash
echo '${UNSET:-default}' | ./envsubst --debug 2>&1

# 输出:
# [DEBUG] Use default: ${UNSET:-default} -> default
# default
```

### 实际应用场景

**场景 1: Docker 多环境配置**

```dockerfile
# Dockerfile
FROM nginx:alpine
COPY nginx.conf.tpl /etc/nginx/
COPY envsubst /usr/local/bin/

CMD envsubst < /etc/nginx/nginx.conf.tpl > /etc/nginx/nginx.conf \
    && nginx -g 'daemon off;'
```

```bash
# 开发环境 - 使用默认值
docker run myapp

# 生产环境 - 覆盖关键配置
docker run -e PORT=443 -e HOST=prod.example.com myapp
```

**场景 2: CI/CD 中的灵活配置**

```yaml
# GitHub Actions
- name: Deploy
  run: |
    ./envsubst << 'EOF' > deploy.yaml
    apiVersion: apps/v1
    kind: Deployment
    metadata:
      name: ${APP_NAME:-myapp}
    spec:
      replicas: ${REPLICAS:-3}
      template:
        spec:
          containers:
          - image: ${IMAGE:-myapp:latest}
            ports:
            - containerPort: ${PORT:-8080}
    EOF
```

---

## 2️⃣ 配置文件支持 `--whitelist-file`

### 基本用法

```bash
./envsubst --whitelist-file rules.txt < template.conf
```

### 文件格式

```text
# 这是注释，会被忽略
REST_*
WAF_*
APP_*

# 空行也会被忽略
DB_*
```

**规则**:
- 每行一个规则
- 支持通配符 (`*`)
- `#` 开头的行为注释
- 空行被忽略
- 自动去除首尾空白

### 使用示例

#### 示例 1: 创建白名单文件

```bash
cat > whitelist.txt << 'EOF'
# Application variables
APP_*

# Database variables  
DB_*
REDIS_*

# WAF configuration
WAF_*
EOF
```

#### 示例 2: 使用白名单文件

```bash
# 模板文件
cat > config.tpl << 'EOF'
app_name=${APP_NAME}
db_host=${DB_HOST}
redis_url=${REDIS_URL}
waf_enabled=${WAF_ENABLED}
system_var=${SYSTEM_VAR}
EOF

# 使用白名单文件
export APP_NAME=myapp DB_HOST=localhost REDIS_URL=redis://localhost WAF_ENABLED=true SYSTEM_VAR=secret

./envsubst --whitelist-file whitelist.txt < config.tpl

# 输出:
# app_name=myapp
# db_host=localhost
# redis_url=redis://localhost
# waf_enabled=true
# system_var=${SYSTEM_VAR}  ← 不在白名单中，保持原样
```

#### 示例 3: 结合调试模式

```bash
./envsubst --debug --whitelist-file whitelist.txt < config.tpl 2>&1

# 输出:
# [DEBUG] Loaded 5 rules from 'whitelist.txt'
# [DEBUG] Replace: ${APP_NAME} -> myapp
# [DEBUG] Replace: ${DB_HOST} -> localhost
# [DEBUG] Replace: ${REDIS_URL} -> redis://localhost
# [DEBUG] Replace: ${WAF_ENABLED} -> true
# [DEBUG] Skip (not in whitelist): ${SYSTEM_VAR}
```

### 实际应用场景

**场景 1: Kubernetes ConfigMap 管理**

```yaml
# k8s/configmap.yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: app-whitelist
data:
  rules.txt: |
    # Production variables
    PROD_*
    K8S_*
    
    # Monitoring
    METRICS_*
```

```bash
# 在 Pod 中使用
kubectl create configmap app-whitelist --from-file=rules.txt

# Deployment 中挂载
volumeMounts:
  - name: whitelist
    mountPath: /etc/whitelist
volumes:
  - name: whitelist
    configMap:
      name: app-whitelist

# 启动脚本
#!/bin/sh
envsubst --whitelist-file /etc/whitelist/rules.txt \
    < /app/config.tpl > /app/config.yaml
exec /app/main
```

**场景 2: 多租户配置隔离**

```bash
# tenant-a-rules.txt
TENANT_A_*
SHARED_*

# tenant-b-rules.txt
TENANT_B_*
SHARED_*

# 为不同租户生成配置
./envsubst --whitelist-file tenant-a-rules.txt < base.tpl > tenant-a.conf
./envsubst --whitelist-file tenant-b-rules.txt < base.tpl > tenant-b.conf
```

**场景 3: CI/CD 中的动态白名单**

```yaml
# GitLab CI
generate_config:
  script:
    # 根据环境动态生成白名单
    - |
      if [ "$CI_ENVIRONMENT" = "production" ]; then
        cat > rules.txt << EOF
        PROD_*
        MONITORING_*
        EOF
      else
        cat > rules.txt << EOF
        DEV_*
        TESTING_*
        EOF
      fi
    
    - ./envsubst --whitelist-file rules.txt < app.tpl > app.conf
```

### 高级技巧

#### 组合命令行参数和文件

```bash
# 文件中的规则 + 命令行额外规则
./envsubst --whitelist-file base-rules.txt 'EXTRA_*' < template.conf
```

#### 多个白名单文件

```bash
# 合并多个文件的规则（需要手动合并）
cat rules1.txt rules2.txt > combined-rules.txt
./envsubst --whitelist-file combined-rules.txt < template.conf
```

---

## 3️⃣ JSON 输出格式 `--json-stats`

### 基本用法

```bash
./envsubst --json-stats < template.conf 2>stats.json
```

### JSON 格式

```json
{
  "envsubst_stats": {
    "variables_replaced": 5,
    "variables_kept": 2,
    "variables_skipped": 3,
    "total_processed": 10
  }
}
```

### 字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| `variables_replaced` | integer | 成功替换的变量数量 |
| `variables_kept` | integer | 保留的未定义变量数量（-k 模式） |
| `variables_skipped` | integer | 因白名单跳过的变量数量 |
| `total_processed` | integer | 处理的变量总数 |

### 使用示例

#### 示例 1: 基本 JSON 输出

```bash
echo '${A} ${B} ${C}' | A=1 B=2 ./envsubst --json-stats 2>&1

# 输出到 stderr:
# {
#   "envsubst_stats": {
#     "variables_replaced": 3,
#     "variables_kept": 0,
#     "variables_skipped": 0,
#     "total_processed": 3
#   }
# }
```

#### 示例 2: 捕获 JSON 用于自动化

```bash
#!/bin/bash

# 生成配置并捕获统计
./envsubst --json-stats 'PROD_*' < config.tpl > config.yaml 2>stats.json

# 解析 JSON（使用 jq）
SKIPPED=$(jq '.envsubst_stats.variables_skipped' stats.json)
REPLACED=$(jq '.envsubst_stats.variables_replaced' stats.json)

echo "Replaced: $REPLACED, Skipped: $SKIPPED"

# 如果有变量被跳过，发出警告
if [ "$SKIPPED" -gt 0 ]; then
    echo "⚠️  Warning: $SKIPPED variables were skipped!"
    exit 1
fi
```

#### 示例 3: CI/CD 集成

```yaml
# GitHub Actions
- name: Generate Configuration
  run: |
    ./envsubst --json-stats 'APP_* DB_*' < template.yaml > config.yaml 2>stats.json
    
  # Upload stats as artifact
- name: Upload Stats
  uses: actions/upload-artifact@v3
  with:
    name: substitution-stats
    path: stats.json
    
  # Validate no unexpected skips
- name: Validate Substitution
  run: |
    SKIPPED=$(jq '.envsubst_stats.variables_skipped' stats.json)
    if [ "$SKIPPED" != "0" ]; then
      echo "::error::Unexpected skipped variables: $SKIPPED"
      exit 1
    fi
```

#### 示例 4: 监控和告警

```bash
#!/bin/bash
# monitoring-check.sh

./envsubst --json-stats 'PROD_*' < config.tpl > /dev/null 2>stats.json

TOTAL=$(jq '.envsubst_stats.total_processed' stats.json)
SKIPPED=$(jq '.envsubst_stats.variables_skipped' stats.json)
SKIP_RATE=$((SKIPPED * 100 / TOTAL))

# 如果跳过率超过 10%，发送告警
if [ "$SKIP_RATE" -gt 10 ]; then
    curl -X POST https://hooks.slack.com/services/XXX \
        -d "{\"text\": \"⚠️ High skip rate: ${SKIP_RATE}%\"}"
fi

# 记录指标
echo "envsubst_skip_rate=$SKIP_RATE" >> /var/log/metrics.log
```

#### 示例 5: 与默认值结合

```bash
echo '${HOST:-localhost} ${PORT:-8080}' | ./envsubst --json-stats 2>&1

# JSON 输出:
# {
#   "envsubst_stats": {
#     "variables_replaced": 2,    # 使用了默认值也算替换
#     "variables_kept": 0,
#     "variables_skipped": 0,
#     "total_processed": 2
#   }
# }
```

### 与其他工具集成

#### 使用 Python 解析

```python
import json
import subprocess

# 运行 envsubst
result = subprocess.run(
    ['./envsubst', '--json-stats', 'APP_*'],
    input='template content',
    capture_output=True,
    text=True
)

# 解析 JSON 统计
stats = json.loads(result.stderr)
print(f"Replaced: {stats['envsubst_stats']['variables_replaced']}")
```

#### 使用 Node.js 解析

```javascript
const { exec } = require('child_process');

exec('./envsubst --json-stats APP_* < template.conf', 
  (error, stdout, stderr) => {
    const stats = JSON.parse(stderr);
    console.log(`Total processed: ${stats.envsubst_stats.total_processed}`);
  }
);
```

---

## 🎨 功能组合使用

### 组合 1: 默认值 + JSON 统计

```bash
echo '${HOST:-localhost} ${PORT:-80}' | ./envsubst --json-stats 2>&1

# 输出:
# localhost 80
# {
#   "envsubst_stats": {
#     "variables_replaced": 2,
#     "variables_kept": 0,
#     "variables_skipped": 0,
#     "total_processed": 2
#   }
# }
```

### 组合 2: 白名单文件 + 调试 + JSON

```bash
./envsubst --debug --json-stats --whitelist-file rules.txt < config.tpl 2>&1

# stderr 输出:
# [DEBUG] Loaded 3 rules from 'rules.txt'
# [DEBUG] Replace: ${APP_HOST} -> localhost
# [DEBUG] Skip (not in whitelist): ${SYSTEM_VAR}
# {
#   "envsubst_stats": {
#     "variables_replaced": 1,
#     "variables_kept": 0,
#     "variables_skipped": 1,
#     "total_processed": 2
#   }
# }
```

### 组合 3: 完整的生产场景

```bash
#!/bin/bash
# production-deploy.sh

set -e

WHITELIST="/etc/app/whitelist.txt"
TEMPLATE="/app/config.tpl"
OUTPUT="/app/config.yaml"
STATS="/tmp/envsubst-stats.json"

# 生成配置
./envsubst \
    --whitelist-file "$WHITELIST" \
    --json-stats \
    < "$TEMPLATE" > "$OUTPUT" 2>"$STATS"

# 验证结果
SKIPPED=$(jq '.envsubst_stats.variables_skipped' "$STATS")
if [ "$SKIPPED" -gt 0 ]; then
    echo "❌ Error: $SKIPPED variables were skipped"
    jq '.' "$STATS"
    exit 1
fi

echo "✅ Configuration generated successfully"
jq '.' "$STATS"

# 重启服务
systemctl restart myapp
```

---

## 📊 性能对比

| 功能 | 性能影响 | 建议 |
|------|---------|------|
| 默认值 | 轻微 (~5%) | 可放心使用 |
| 白名单文件 | 一次性加载 | 大文件 (>1000行) 注意内存 |
| JSON 统计 | 轻微 (~2%) | CI/CD 中推荐使用 |
| 调试模式 | 显著 I/O | 仅开发/排查问题时使用 |

---

## 🔧 故障排查

### 问题 1: 默认值不生效

**检查**:
```bash
# 确保语法正确
echo '${VAR:-default}' | ./envsubst --debug 2>&1

# 常见错误:
# ${VAR-default}     ❌ 缺少冒号
# ${VAR: -default}   ❌ 有空格
# ${VAR:-default}    ✅ 正确
```

### 问题 2: 白名单文件加载失败

**检查**:
```bash
# 文件是否存在
ls -l rules.txt

# 文件权限
chmod 644 rules.txt

# 文件格式（检查隐藏字符）
cat -A rules.txt

# 使用调试模式查看加载情况
./envsubst --debug --whitelist-file rules.txt 2>&1 | grep "Loaded"
```

### 问题 3: JSON 解析失败

**检查**:
```bash
# 确保重定向 stderr
./envsubst --json-stats < tpl.conf 2>stats.json

# 验证 JSON 格式
jq '.' stats.json

# 常见错误:
# ./envsubst --json-stats < tpl.conf > stats.json  ❌ 错误，stdout 是配置内容
# ./envsubst --json-stats < tpl.conf 2>stats.json  ✅ 正确，stderr 是 JSON
```

---

## 💡 最佳实践

1. **默认值**: 为非关键配置提供合理的默认值
2. **白名单文件**: 将规则纳入版本控制，便于审查
3. **JSON 统计**: 在 CI/CD 中始终启用，用于质量检查
4. **调试模式**: 仅在开发和问题排查时启用
5. **组合使用**: 根据场景灵活组合功能

---

## 📚 相关资源

- [README.md](README.md) - 基础功能文档
- [ADVANCED_FEATURES_GUIDE.md](ADVANCED_FEATURES_GUIDE.md) - 高级功能文档
- [GitHub Issues](https://github.com/tekintian/envsubst/issues) - 报告问题

---

**版本**: v1.0.0  
**更新日期**: 2026-04-18