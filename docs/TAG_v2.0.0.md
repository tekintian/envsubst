# envsubst v2.0.0 - 原生就地编辑功能

## 🎉 重大更新

从 v2.0.0 开始，envsubst **原生支持就地编辑（in-place editing）**功能，无需额外的脚本或工具！

---

## ✨ 新特性

### 1. 原生 `-i` / `--in-place` 选项

```bash
# 基本就地编辑
./envsubst -i 'APP_* DB_*' config.conf

# 带备份的就地编辑
./envsubst -i.bak 'APP_* DB_*' config.conf

# 自定义备份后缀
./envsubst -i.20250101 'APP_*' config.conf

# 无白名单（全部替换）
./envsubst -i config.conf
```

**核心优势**：
- ✅ 语法简洁优雅（类似 `sed -i`）
- ✅ 原子操作，安全可靠
- ✅ 内置错误处理
- ✅ 自动清理临时文件
- ✅ 可选备份功能
- ✅ 单二进制文件，零依赖

### 2. 实现细节

- 使用 `mkstemp()` 安全创建临时文件
- 使用 `link()` 创建硬链接备份（快速、原子）
- 使用 `rename()` 进行原子文件替换
- 完整的错误处理和资源清理
- 内存泄漏防护（正确释放 backup 字符串）

---

## 🔧 技术改进

### 代码变更

**新增头文件**：
- `<unistd.h>` - mkstemp, link
- `<sys/stat.h>` - stat, S_ISREG
- `<libgen.h>` - 路径处理
- `<limits.h>` - PATH_MAX 定义

**扩展结构体**：
```c
struct envsubst_ctx {
    // ... 原有字段
    bool in_place;           // 就地编辑模式
    char* in_place_backup;   // 备份后缀
};
```

**新增函数**：
- `process_in_place()` - 就地编辑核心逻辑

**更新函数**：
- `ctx_init()` - 初始化新字段
- `ctx_free()` - 释放 backup 字符串
- `usage()` - 添加 `-i` 选项说明
- `main()` - 支持就地编辑模式

### 安全性保证

1. **原子操作**：临时文件 + rename，不会出现半写入状态
2. **错误回滚**：任何步骤失败都会清理临时文件，原文件不受影响
3. **资源管理**：正确使用 trap 和 cleanup，无内存泄漏
4. **权限保持**：rename 保持文件权限不变

---

## 📊 对比 v1.x

| 特性 | v1.x | v2.0.0 |
|------|------|--------|
| 就地编辑 | ❌ 需要手动管理临时文件 | ✅ 原生支持 `-i` |
| 备份功能 | ❌ 需要额外命令 | ✅ `-i.bak` 一键备份 |
| 易用性 | ⭐⭐ 繁琐 | ⭐⭐⭐⭐⭐ 简洁 |
| 安全性 | ⚠️ 容易出错 | ✅ 内置保护 |
| 文件数量 | 1 个二进制 | 1 个二进制 |
| 依赖 | 无 | 无 |

---

## 🚀 使用场景

### Docker 容器

```dockerfile
FROM nginx:alpine
COPY envsubst /usr/local/bin/

# 推荐做法（v2.0.0+）
CMD ./envsubst -i.bak 'NGINX_* APP_*' /etc/nginx/nginx.conf \
    && nginx -g 'daemon off;'
```

### CI/CD 部署

```yaml
# GitHub Actions
- name: Update config
  run: ./envsubst -i.$(date +%Y%m%d) 'PROD_*' config.yaml
```

### 批量处理

```bash
# 更新所有配置文件
for conf in configs/*.conf; do
    ./envsubst -i.bak 'APP_*' "$conf"
done
```

### Kubernetes

```yaml
initContainers:
- name: config-generator
  command: ['sh', '-c']
  args:
  - |
    /usr/local/bin/envsubst -i 'APP_* DB_*' /config/app.conf
```

---

## 📝 完整示例

### 示例 1: Nginx 配置

```bash
# 模板文件 nginx.conf.tpl
server {
    listen ${PORT:-80};
    server_name ${HOST:-localhost};
    
    location / {
        proxy_pass http://${BACKEND:-127.0.0.1:8080};
    }
}

# 就地编辑（带备份）
export PORT=8080
export HOST=example.com
export BACKEND=http://api:3000

./envsubst -i.bak 'PORT HOST BACKEND' nginx.conf.tpl

# 结果：
# nginx.conf.tpl      ← 新内容（已替换变量）
# nginx.conf.tpl.bak  ← 原始模板
```

### 示例 2: 组合其他选项

```bash
# 就地编辑 + 调试模式
./envsubst -i --debug 'APP_*' config.conf

# 就地编辑 + 统计信息
./envsubst -i --stats 'APP_*' config.conf 2>stats.json

# 就地编辑 + 保留未定义变量
./envsubst -i -k 'APP_*' config.conf

# 就地编辑 + JSON 统计
./envsubst -i --json-stats 'APP_*' config.conf 2>stats.json
```

---

## 🐛 Bug 修复

- 修复 `ctx_init()` 未初始化 `in_place` 和 `in_place_backup` 的问题
- 修复 `ctx_free()` 未释放 `in_place_backup` 导致的内存泄漏
- 正确处理 `optional_argument` 的 NULL optarg

---

## 📚 相关文档

- [IN_PLACE_EDITING.md](docs/IN_PLACE_EDITING.md) - 就地编辑完整指南
- [SPECIAL_SCENARIOS.md](docs/SPECIAL_SCENARIOS.md) - 特殊场景分析（已更新）
- [README.md](README.md) - 项目主文档

---

## ⬆️ 升级指南

### 从 v1.x 升级

**之前的方式**：
```bash
# v1.x - 繁琐且容易出错
./envsubst < config.tpl > config.tmp && mv config.tmp config.tpl
```

**现在的方式**：
```bash
# v2.0.0 - 简洁优雅
./envsubst -i 'APP_*' config.tpl
```

**兼容性**：
- ✅ 完全向后兼容
- ✅ 所有 v1.x 的命令仍然有效
- ✅ 新的 `-i` 选项是可选的

---

## 🎯 为什么选择 v2.0.0？

1. **更优雅**：一行命令完成就地编辑
2. **更安全**：内置原子操作和错误处理
3. **更简单**：无需学习临时文件管理
4. **更可靠**：C 语言实现，经过充分测试
5. **零依赖**：单个二进制文件，无外部依赖

---

## 📦 安装

### 从源码编译

```bash
git clone https://github.com/tekintian/envsubst.git
cd envsubst
make
sudo make install
```

### 验证安装

```bash
envsubst --version
# 输出: envsubst v2.0.0

envsubst --help | grep "in-place"
# 输出: -i, --in-place[=SUFFIX] 就地编辑文件（可选备份后缀）
```

---

## 🙏 致谢

感谢所有提出就地编辑需求的用户，这个功能让 envsubst 更加完善和易用！

---

**Full Changelog**: https://github.com/tekintian/envsubst/compare/v1.0.0...v2.0.0
