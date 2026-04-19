# envsubst 特殊场景分析

## 场景 1: 同文件读写 `envsubst < file > file`

### ❌ 问题：直接重定向会导致文件清空

```bash
# 这种方式会失败！
echo '${VAR}' > config.txt
VAR=value ./envsubst < config.txt > config.txt
# 结果：config.txt 变成空文件！
```

### 🔍 原因分析

这是 **Shell 重定向机制**的问题，不是 envsubst 的 bug：

1. Shell 解析命令时，先处理输出重定向 `> config.txt`
2. `>` 操作符会**立即清空**目标文件
3. 然后才执行 `< config.txt` 读取（但文件已经被清空）
4. envsubst 读取到空输入，输出也是空

**执行顺序**：
```
1. Shell: > config.txt  (清空文件)
2. Shell: < config.txt  (读取空文件)
3. envsubst: 处理空输入
4. envsubst: 输出空内容
```

### ✅ 解决方案

#### 方案 1: 使用原生 `-i` 选项（⭐⭐⭐⭐⭐ 强烈推荐）

从 **v2.0.0** 开始，envsubst 原生支持就地编辑功能：

```bash
# 基本就地编辑
./envsubst -i 'APP_* DB_*' config.conf

# 带备份的就地编辑
./envsubst -i.bak 'APP_* DB_*' config.conf

# 自定义备份后缀
./envsubst -i.20250101 'APP_*' config.conf
```

**优点**：
- ✅ 语法简洁优雅（类似 `sed -i`）
- ✅ 原子操作，安全可靠
- ✅ 内置错误处理
- ✅ 自动清理临时文件
- ✅ 可选备份功能
- ✅ 单二进制文件，无需额外脚本

**缺点**：
- ⚠️ 需要 v2.0.0 或更高版本

**详细说明**：参见 [IN_PLACE_EDITING.md](IN_PLACE_EDITING.md)

---

#### 方案 2: 使用临时文件（传统方式）

```bash
# 最安全可靠的方式
./envsubst < config.tpl > config.tmp && mv config.tmp config.tpl
```

**优点**：
- ✅ 所有系统都支持
- ✅ 原子操作（mv 是原子的）
- ✅ 如果 envsubst 失败，原文件不受影响

**适用场景**：
- 旧版本 envsubst（v1.x）
- 需要最大兼容性

---

#### 方案 3: 使用 sponge (moreutils)

```bash
# 安装 sponge
brew install moreutils        # macOS
apt install moreutils         # Ubuntu/Debian
yum install moreutils         # CentOS/RHEL

# 使用 sponge
./envsubst < config.tpl | sponge config.tpl
```

**sponge 的工作原理**：
1. 读取所有输入到内存
2. 关闭输入文件
3. 写入输出文件

**优点**：
- ✅ 语法简洁
- ✅ 自动处理缓冲

**缺点**：
- ⚠️ 需要安装额外工具
- ⚠️ 大文件会占用较多内存

---

#### 方案 4: 使用变量缓存

```bash
# 小文件适用
content=$(./envsubst < config.tpl)
echo "$content" > config.tpl
```

**优点**：
- ✅ 无需额外工具

**缺点**：
- ❌ 大文件会占用大量内存
- ❌ 可能丢失尾随换行符
- ❌ 二进制文件不安全

---

#### 方案 5: 使用 sed 就地编辑（不推荐用于 envsubst）

```bash
# ⚠️ 不适用于 envsubst，仅作对比
sed -i 's/\${VAR}/value/g' config.tpl
```

**为什么不推荐**：
- envsubst 是流式处理器，不是文本替换工具
- sed 无法处理复杂的环境变量逻辑

### 📊 方案对比

| 方案 | 安全性 | 性能 | 易用性 | 依赖 | 推荐度 |
|------|--------|------|--------|------|--------|
| **envsubst -i** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | 无 | ⭐⭐⭐⭐⭐ |
| 临时文件 + mv | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐ | 无 | ⭐⭐⭐⭐ |
| sponge | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ | moreutils | ⭐⭐⭐ |
| 变量缓存 | ⭐⭐ | ⭐⭐ | ⭐⭐⭐ | 无 | ⭐⭐ |
| 直接重定向 | ❌ | - | ⭐ | 无 | ❌ |

### 💡 最佳实践

#### Docker 容器中的最佳实践

```dockerfile
# ✅ 推荐做法（v2.0.0+）
CMD ./envsubst -i.bak 'NGINX_* APP_*' /etc/nginx/nginx.conf \
    && nginx -g 'daemon off;'

# ✅ 传统做法（v1.x 或需要最大兼容性）
CMD ./envsubst < /etc/nginx/nginx.conf.template > /etc/nginx/nginx.conf.tmp \
    && mv /etc/nginx/nginx.conf.tmp /etc/nginx/nginx.conf \
    && nginx -g 'daemon off;'
```

#### Shell 脚本中的使用示例

```bash
#!/bin/bash
# 现代方式（v2.0.0+）
./envsubst -i.bak 'APP_* DB_*' config.conf

# 传统方式（v1.x）
safe_envsubst() {
    local template="$1"
    local whitelist="${2:-}"
    local tmpfile
    
    tmpfile=$(mktemp "${template}.XXXXXX")
    trap "rm -f '$tmpfile'" EXIT
    
    if [ -n "$whitelist" ]; then
        envsubst "$whitelist" < "$template" > "$tmpfile"
    else
        envsubst < "$template" > "$tmpfile"
    fi
    
    mv "$tmpfile" "$template"
    trap - EXIT
}

safe_envsubst config.tpl 'APP_* DB_*'
```

---

## 场景 2: 变量值为正则表达式

### ✅ 完全支持！

envsubst **完美支持**正则表达式作为变量值，因为：

1. **envsubst 不做任何转义或解释**
2. 它只是简单的字符串替换
3. 变量的值原封不动地插入模板

### 🧪 测试用例

#### 测试 1: 简单正则

```bash
export VAR1='a|b|c'
echo '${VAR1}' | ./envsubst
# 输出: a|b|c ✅
```

#### 测试 2: 复杂正则

```bash
export VAR2='^(\d+){5,}$'
echo '${VAR2}' | ./envsubst
# 输出: ^(\d+){5,}$ ✅
```

#### 测试 3: 邮箱正则

```bash
export EMAIL_REGEX='^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$'
echo '${EMAIL_REGEX}' | ./envsubst
# 输出: ^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$ ✅
```

#### 测试 4: Nginx location 正则

```bash
export LOCATION_REGEX='~* \.(jpg|jpeg|png|gif|ico)$'

cat > nginx.tpl << 'EOF'
location ${LOCATION_REGEX} {
    expires 30d;
    add_header Cache-Control "public";
}
EOF

./envsubst < nginx.tpl
# 输出:
# location ~* \.(jpg|jpeg|png|gif|ico)$ {
#     expires 30d;
#     add_header Cache-Control "public";
# }
# ✅
```

#### 测试 5: JSON 配置中的正则

```bash
export PHONE_PATTERN='^\d{3}-\d{4}$'

cat > config.json.tpl << 'EOF'
{
    "validation": {
        "phone_pattern": "${PHONE_PATTERN}",
        "description": "Phone number pattern"
    }
}
EOF

./envsubst < config.json.tpl
# 输出:
# {
#     "validation": {
#         "phone_pattern": "^\d{3}-\d{4}$",
#         "description": "Phone number pattern"
#     }
# }
# ✅
```

### 🎯 实际应用场景

#### 场景 1: Nginx 配置管理

```nginx
# nginx.conf.template
server {
    listen ${PORT:-80};
    
    # 静态资源缓存
    location ${STATIC_REGEX} {
        expires ${CACHE_EXPIRY:-30d};
        add_header Cache-Control "public, immutable";
    }
    
    # API 路由
    location ${API_REGEX} {
        proxy_pass ${BACKEND_URL};
    }
}
```

```bash
# 环境变量
export STATIC_REGEX='~* \.(css|js|jpg|png|gif)$'
export API_REGEX='^/api/v[0-9]+'
export BACKEND_URL='http://api-server:8080'
export CACHE_EXPIRY='7d'

# 生成配置
./envsubst 'STATIC_REGEX API_REGEX BACKEND_URL CACHE_EXPIRY PORT' \
    < nginx.conf.template > nginx.conf
```

#### 场景 2: 应用配置文件

```yaml
# app.config.yaml
validation:
  email_pattern: "${EMAIL_REGEX}"
  phone_pattern: "${PHONE_REGEX}"
  username_pattern: "${USERNAME_REGEX}"

routes:
  admin: "${ADMIN_ROUTE_REGEX}"
  api: "${API_ROUTE_REGEX}"
```

```bash
# 环境变量
export EMAIL_REGEX='^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$'
export PHONE_REGEX='^\+?[1-9]\d{1,14}$'
export USERNAME_REGEX='^[a-zA-Z][a-zA-Z0-9_]{2,19}$'
export ADMIN_ROUTE_REGEX='^/admin(/.*)?$'
export API_ROUTE_REGEX='^/api/v[0-9]+(/.*)?$'

# 生成配置
./envsubst < app.config.yaml.template > app.config.yaml
```

#### 场景 3: Kubernetes Ingress

```yaml
# ingress.yaml.template
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: ${APP_NAME}-ingress
  annotations:
    nginx.ingress.kubernetes.io/rewrite-target: /
spec:
  rules:
  - host: ${DOMAIN}
    http:
      paths:
      - path: ${STATIC_PATH}
        pathType: ImplementationSpecific
        backend:
          service:
            name: static-service
            port:
              number: 80
      - path: ${API_PATH}
        pathType: ImplementationSpecific
        backend:
          service:
            name: api-service
            port:
              number: 8080
```

```bash
export APP_NAME='myapp'
export DOMAIN='example.com'
export STATIC_PATH='~* \.(css|js|png|jpg)$'
export API_PATH='/api/.*'

./envsubst < ingress.yaml.template > ingress.yaml
```

### ⚠️ 注意事项

#### 1. Shell 转义

在设置包含特殊字符的正则时，注意 Shell 转义：

```bash
# ✅ 正确：使用单引号
export REGEX='^\d+$'

# ❌ 错误：双引号可能导致问题
export REGEX="^\d+$"  # \d 可能被解释

# ✅ 正确：使用 heredoc
export REGEX<<'EOF'
^\d+$
EOF
```

#### 2. YAML/JSON 中的引号

在 YAML/JSON 中，正则表达式通常需要引号：

```yaml
# ✅ 正确
pattern: "${REGEX}"

# ❌ 错误（某些正则可能导致 YAML 解析错误）
pattern: ${REGEX}
```

```json
{
    "pattern": "${REGEX}"  // ✅ 总是加引号
}
```

#### 3. Nginx 正则的特殊性

Nginx 的正则有特殊语法：

```nginx
# ~  区分大小写匹配
# ~* 不区分大小写匹配
# ^~ 前缀匹配（非正则）
# = 精确匹配

location ~* \.(jpg|png)$ { }   # ✅ 正则
location ^~ /static/ { }       # ✅ 前缀
location = /exact { }          # ✅ 精确
```

确保在模板中使用正确的语法。

### 📊 支持的字符

envsubst **原样保留**所有字符，包括：

| 字符类型 | 示例 | 支持 |
|---------|------|------|
| 管道符 | `a\|b\|c` | ✅ |
| 脱字符 | `^start` | ✅ |
| 美元符 | `end$` | ✅ |
| 反斜杠 | `\d \w \s` | ✅ |
| 方括号 | `[a-z]` | ✅ |
| 圆括号 | `(abc)` | ✅ |
| 花括号 | `{3,5}` | ✅ |
| 星号 | `a*` | ✅ |
| 加号 | `a+` | ✅ |
| 问号 | `a?` | ✅ |
| 点号 | `.` | ✅ |
| 连字符 | `-` | ✅ |
| 下划线 | `_` | ✅ |
| @符号 | `@` | ✅ |
| 百分号 | `%` | ✅ |

**结论**：envsubst 对变量值没有任何限制或处理，完全透传！

---

## 总结

### 场景 1: 同文件读写

- ❌ **不支持**直接 `envsubst < file > file`
- ✅ **强烈推荐**使用原生 `-i` 选项：`envsubst -i 'PATTERN' file`（v2.0.0+）
- ✅ **传统方式**使用临时文件：`envsubst < file > file.tmp && mv file.tmp file`
- ✅ **可选**使用 sponge：`envsubst < file | sponge file`

### 场景 2: 正则表达式

- ✅ **完全支持**所有正则表达式
- ✅ **原样保留**所有特殊字符
- ✅ **适用于** Nginx、YAML、JSON、各种配置文件
- ⚠️ **注意** Shell 转义和格式文件的引号要求

---

## 常见问题 FAQ

### Q1: 为什么不能直接同文件读写？

**A**: 这是 Shell 的设计，不是 envsubst 的问题。Shell 在执行命令前先处理重定向，`>` 会立即清空文件。

### Q2: 有没有办法让 envsubst 支持原地编辑？

**A**: **有！** 从 v2.0.0 开始，envsubst 原生支持 `-i` / `--in-place` 选项：

```bash
# 基本用法
./envsubst -i 'APP_*' config.conf

# 带备份
./envsubst -i.bak 'APP_*' config.conf
```

详见 [IN_PLACE_EDITING.md](IN_PLACE_EDITING.md) 完整指南。

### Q3: 正则表达式中的 `$` 会和变量冲突吗？

**A**: 不会！envsubst 只识别 `${VAR}` 和 `$VAR` 格式的变量。正则中的 `$` 单独出现时不会被误认为变量。

```bash
export REGEX='^test$'
echo '${REGEX}' | ./envsubst
# 输出: ^test$ ✅
```

### Q4: 如果正则中包含 `${...}` 怎么办？

**A**: 这种情况很少见，但如果真的需要，可以：

1. 拆分变量：
```bash
export PREFIX='^test'
export SUFFIX='$'
echo "${PREFIX}\${SUFFIX}"  # 手动拼接
```

2. 或者不使用 envsubst 处理这部分

### Q5: 性能如何？大正则会影响性能吗？

**A**: 完全不会。envsubst 只做字符串替换，不解析或验证正则表达式。无论正则多复杂，性能都一样。

---

**参考资料**：
- [Bash 重定向详解](https://www.gnu.org/software/bash/manual/html_node/Redirections.html)
- [Nginx Location 匹配规则](http://nginx.org/en/docs/http/ngx_http_core_module.html#location)
- [Regular Expressions in NGINX](https://www.nginx.com/blog/creating-nginx-rewrite-rules/)
