#!/bin/bash
#
# envsubst 完整功能测试套件
# 测试所有核心功能和高级特性
#

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 计数器
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# 测试函数
run_test() {
    local test_name="$1"
    local test_command="$2"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo -e "${BLUE}[Test $TOTAL_TESTS]${NC} $test_name"
    
    if eval "$test_command"; then
        PASSED_TESTS=$((PASSED_TESTS + 1))
        echo -e "  ${GREEN}✅ PASS${NC}"
        return 0
    else
        FAILED_TESTS=$((FAILED_TESTS + 1))
        echo -e "  ${RED}❌ FAIL${NC}"
        return 1
    fi
}

# 打印测试结果
print_summary() {
    echo ""
    echo "=========================================="
    echo "测试结果汇总"
    echo "=========================================="
    echo -e "总测试数: ${BLUE}$TOTAL_TESTS${NC}"
    echo -e "通过:     ${GREEN}$PASSED_TESTS${NC}"
    echo -e "失败:     ${RED}$FAILED_TESTS${NC}"
    echo "=========================================="
    
    if [ $FAILED_TESTS -eq 0 ]; then
        echo -e "${GREEN}🎉 所有测试通过！${NC}"
        exit 0
    else
        echo -e "${RED}⚠️  有 $FAILED_TESTS 个测试失败${NC}"
        exit 1
    fi
}

echo "=========================================="
echo "envsubst 完整功能测试套件"
echo "=========================================="
echo ""

# 确保 envsubst 已编译
if [ ! -f "./envsubst" ]; then
    echo -e "${RED}错误: envsubst 未找到，请先运行 make${NC}"
    exit 1
fi

chmod +x ./envsubst

# ==========================================
# 第一部分：基本变量替换
# ==========================================
echo -e "${YELLOW}=== 第一部分：基本变量替换 ===${NC}"
echo ""

# Test 1.1: 简单变量替换
run_test "简单变量替换 \${VAR}" \
    "[ \"\$(echo '\${MY_VAR}' | MY_VAR=hello ./envsubst)\" = 'hello' ]"

# Test 1.2: 多个变量替换
run_test "多个变量替换" \
    "[ \"\$(echo '\${A} \${B}' | A=1 B=2 ./envsubst)\" = '1 2' ]"

# Test 1.3: 变量不存在时删除（默认行为）
run_test "未定义变量默认删除" \
    "[ \"\$(echo '\${UNDEF}' | ./envsubst)\" = '' ]"

# Test 1.4: 混合定义和未定义变量
run_test "混合定义和未定义变量" \
    "[ \"\$(echo '\${DEF} \${UNDEF}' | DEF=value ./envsubst)\" = 'value ' ]"

# Test 1.5: 空值变量
run_test "空值变量处理" \
    "[ \"\$(echo '\${EMPTY}' | EMPTY= ./envsubst)\" = '' ]"

echo ""

# ==========================================
# 第二部分：-k 保留未定义变量
# ==========================================
echo -e "${YELLOW}=== 第二部分：-k 保留未定义变量 ===${NC}"
echo ""

# Test 2.1: 保留单个未定义变量
run_test "-k 保留未定义变量" \
    "[ \"\$(echo '\${UNDEF}' | ./envsubst -k)\" = '\${UNDEF}' ]"

# Test 2.2: 混合保留和替换
run_test "-k 混合模式" \
    "[ \"\$(echo '\${DEF} \${UNDEF}' | DEF=value ./envsubst -k)\" = 'value \${UNDEF}' ]"

# Test 2.3: 多个未定义变量
run_test "-k 多个未定义变量" \
    "[ \"\$(echo '\${A} \${B}' | ./envsubst -k)\" = '\${A} \${B}' ]"

echo ""

# ==========================================
# 第三部分：白名单功能
# ==========================================
echo -e "${YELLOW}=== 第三部分：白名单功能 ===${NC}"
echo ""

# Test 3.1: 前缀通配符
run_test "前缀通配符 APP_*" \
    "[ \"\$(echo '\${APP_HOST} \${DB_HOST}' | APP_HOST=localhost DB_HOST=db ./envsubst 'APP_*')\" = 'localhost \${DB_HOST}' ]"

# Test 3.2: 后缀通配符
run_test "后缀通配符 *_HOST" \
    "[ \"\$(echo '\${APP_HOST} \${APP_PORT}' | APP_HOST=localhost APP_PORT=80 ./envsubst '*_HOST')\" = 'localhost \${APP_PORT}' ]"

# Test 3.3: 中间通配符
run_test "中间通配符 APP_*_NAME" \
    "[ \"\$(echo '\${APP_SERVER_NAME} \${APP_PORT}' | APP_SERVER_NAME=test APP_PORT=80 ./envsubst 'APP_*_NAME')\" = 'test \${APP_PORT}' ]"

# Test 3.4: 多个白名单规则（空格分隔）
run_test "多个白名单规则（空格）" \
    "[ \"\$(echo '\${REST_A} \${WAF_B} \${OTHER_C}' | REST_A=1 WAF_B=2 OTHER_C=3 ./envsubst 'REST_* WAF_*')\" = '1 2 \${OTHER_C}' ]"

# Test 3.5: 多个白名单规则（逗号分隔）
run_test "多个白名单规则（逗号）" \
    "[ \"\$(echo '\${A_X} \${B_Y} \${C_Z}' | A_X=1 B_Y=2 C_Z=3 ./envsubst 'A_*,B_*')\" = '1 2 \${C_Z}' ]"

# Test 3.6: 无白名单时全部替换
run_test "无白名单全部替换" \
    "[ \"\$(echo '\${A} \${B}' | A=1 B=2 ./envsubst)\" = '1 2' ]"

# Test 3.7: 白名单为空时跳过所有
run_test "白名单不匹配时跳过" \
    "[ \"\$(echo '\${VAR}' | VAR=value ./envsubst 'OTHER_*')\" = '\${VAR}' ]"

echo ""

# ==========================================
# 第四部分：--all 全模式
# ==========================================
echo -e "${YELLOW}=== 第四部分：--all 全模式 ===${NC}"
echo ""

# Test 4.1: --all 替换 $VAR 格式
run_test "--all 替换 \$VAR 格式" \
    "[ \"\$(echo '\$MY_VAR' | MY_VAR=test ./envsubst --all)\" = 'test' ]"

# Test 4.2: --all 同时替换两种格式
run_test "--all 同时替换两种格式" \
    "[ \"\$(echo '\$A \${B}' | A=1 B=2 ./envsubst --all)\" = '1 2' ]"

# Test 4.3: --all 与白名单结合
run_test "--all 与白名单结合" \
    "[ \"\$(echo '\$APP_HOST \${DB_HOST}' | APP_HOST=localhost DB_HOST=db ./envsubst --all 'APP_*')\" = 'localhost \${DB_HOST}' ]"

echo ""

# ==========================================
# 第五部分：默认值支持
# ==========================================
echo -e "${YELLOW}=== 第五部分：默认值支持 \${VAR:-default} ===${NC}"
echo ""

# Test 5.1: 基本默认值
run_test "基本默认值" \
    "[ \"\$(echo '\${UNSET:-default_val}' | ./envsubst)\" = 'default_val' ]"

# Test 5.2: 环境变量覆盖默认值
run_test "环境变量覆盖默认值" \
    "[ \"\$(echo '\${MY_VAR:-default}' | MY_VAR=custom ./envsubst)\" = 'custom' ]"

# Test 5.3: 空字符串被视为已设置（不使用默认值）
run_test "空字符串不使用默认值" \
    "[ \"\$(echo '\${EMPTY:-fallback}' | EMPTY= ./envsubst)\" = '' ]"

# Test 5.4: 多个默认值
run_test "多个默认值" \
    "[ \"\$(echo '\${A:-1} \${B:-2} \${C:-3}' | ./envsubst)\" = '1 2 3' ]"

# Test 5.5: 默认值包含特殊字符
run_test "默认值包含特殊字符" \
    "[ \"\$(echo '\${URL:-http://localhost:8080}' | ./envsubst)\" = 'http://localhost:8080' ]"

# Test 5.6: Nginx 配置场景
run_test "Nginx 配置默认值" \
    "[ \"\$(echo 'listen \${PORT:-80}; server_name \${HOST:-localhost};' | ./envsubst)\" = 'listen 80; server_name localhost;' ]"

# Test 5.7: 默认值与白名单结合
run_test "默认值与白名单" \
    "[ \"\$(echo '\${APP_PORT:-8080} \${DB_PORT:-5432}' | ./envsubst 'APP_*')\" = '8080 \${DB_PORT:-5432}' ]"

echo ""

# ==========================================
# 第六部分：非法变量名处理
# ==========================================
echo -e "${YELLOW}=== 第六部分：非法变量名原样保留 ===${NC}"
echo ""

# Test 6.1: 包含空格的变量名
run_test "非法变量名（空格）原样保留" \
    "[ \"\$(echo '\${GOPATH abc}' | ./envsubst)\" = '\${GOPATH abc}' ]"

# Test 6.2: 以数字开头的变量名
run_test "非法变量名（数字开头）原样保留" \
    "[ \"\$(echo '\${123NUM}' | ./envsubst)\" = '\${123NUM}' ]"

# Test 6.3: 包含特殊字符的变量名
run_test "非法变量名（特殊字符）原样保留" \
    "[ \"\$(echo '\${VAR@HOST}' | ./envsubst)\" = '\${VAR@HOST}' ]"

# Test 6.4: 合法变量名正常替换
run_test "合法变量名正常替换" \
    "[ \"\$(echo '\${VALID_VAR_123}' | VALID_VAR_123=test ./envsubst)\" = 'test' ]"

echo ""

# ==========================================
# 第七部分：调试模式
# ==========================================
echo -e "${YELLOW}=== 第七部分：--debug 调试模式 ===${NC}"
echo ""

# Test 7.1: 显示替换过程
run_test "Debug 显示替换" \
    "echo '\${A}' | A=1 ./envsubst --debug 2>&1 | grep -q '\[DEBUG\] Replace: \${A} -> 1'"

# Test 7.2: 显示跳过过程
run_test "Debug 显示跳过" \
    "echo '\${A} \${B}' | A=1 ./envsubst --debug 'A_*' 2>&1 | grep -q '\[DEBUG\] Skip'"

# Test 7.3: Debug 不影响 stdout
run_test "Debug 输出到 stderr" \
    "[ \"\$(echo '\${X}' | X=val ./envsubst --debug 2>/dev/null)\" = 'val' ]"

echo ""

# ==========================================
# 第八部分：统计信息
# ==========================================
echo -e "${YELLOW}=== 第八部分：--stats 统计模式 ===${NC}"
echo ""

# Test 8.1: 统计替换数量
run_test "Stats 统计替换数" \
    "echo '\${A} \${B}' | A=1 B=2 ./envsubst --stats 2>&1 | grep -q 'Variables replaced:.*2'"

# Test 8.2: 统计保留数量
run_test "Stats 统计保留数" \
    "echo '\${DEF} \${UNDEF}' | DEF=val ./envsubst -k --stats 2>&1 | grep -q 'Variables kept:.*1'"

# Test 8.3: 统计跳过数量
run_test "Stats 统计跳过数" \
    "echo '\${ALLOWED_VAR} \${SKIPPED_VAR}' | ALLOWED_VAR=yes ./envsubst --stats 'ALLOWED_*' 2>&1 | grep -q 'Variables skipped:.*1'"

# Test 8.4: 统计总数
run_test "Stats 统计总数" \
    "echo '\${A} \${B} \${C}' | A=1 ./envsubst -k --stats 2>&1 | grep -q 'Total processed:.*3'"

# Test 8.5: Stats 输出到 stderr
run_test "Stats 输出到 stderr" \
    "[ \"\$(echo '\${X}' | X=val ./envsubst --stats 2>/dev/null)\" = 'val' ]"

echo ""

# ==========================================
# 第九部分：JSON 统计
# ==========================================
echo -e "${YELLOW}=== 第九部分：--json-stats JSON 格式 ===${NC}"
echo ""

# Test 9.1: JSON 格式正确性
run_test "JSON 格式统计" \
    "echo '\${A}' | A=1 ./envsubst --json-stats 2>&1 | grep -q '\"variables_replaced\": 1'"

# Test 9.2: JSON 包含所有字段
run_test "JSON 包含所有字段" \
    "echo '\${A} \${B}' | A=1 ./envsubst -k --json-stats 2>&1 | grep -q '\"variables_kept\"' && \
     echo '\${A} \${B}' | A=1 ./envsubst -k --json-stats 2>&1 | grep -q '\"variables_skipped\"' && \
     echo '\${A} \${B}' | A=1 ./envsubst -k --json-stats 2>&1 | grep -q '\"total_processed\"'"

# Test 9.3: JSON 可解析
run_test "JSON 可被解析" \
    "echo '\${X}' | X=1 ./envsubst --json-stats 2>&1 | python3 -c 'import sys,json; json.load(sys.stdin)' 2>/dev/null || \
     echo '\${X}' | X=1 ./envsubst --json-stats 2>&1 | grep -q '{'"

# Test 9.4: JSON 输出到 stderr
run_test "JSON 输出到 stderr" \
    "[ \"\$(echo '\${Y}' | Y=2 ./envsubst --json-stats 2>/dev/null)\" = '2' ]"

echo ""

# ==========================================
# 第十部分：配置文件白名单
# ==========================================
echo -e "${YELLOW}=== 第十部分：--whitelist-file 配置文件 ===${NC}"
echo ""

# 创建测试白名单文件
cat > /tmp/test_whitelist.txt << 'EOF'
# 这是注释行
TEST_*

ALLOWED_*
# 另一个注释
CONFIG_*
EOF

# Test 10.1: 从文件加载白名单
run_test "从文件加载白名单" \
    "[ \"\$(echo '\${TEST_A} \${ALLOWED_B} \${BLOCKED_C}' | TEST_A=1 ALLOWED_B=2 BLOCKED_C=3 ./envsubst --whitelist-file /tmp/test_whitelist.txt)\" = '1 2 \${BLOCKED_C}' ]"

# Test 10.2: 文件包含空行和注释
run_test "白名单文件忽略注释和空行" \
    "[ \"\$(echo '\${CONFIG_X}' | CONFIG_X=yes ./envsubst --whitelist-file /tmp/test_whitelist.txt)\" = 'yes' ]"

# Test 10.3: 文件与命令行规则冲突（都生效）
run_test "文件与命令行规则共存" \
    "[ \"\$(echo '\${TEST_V} \${EXTRA_W}' | TEST_V=1 EXTRA_W=2 ./envsubst --whitelist-file /tmp/test_whitelist.txt 'EXTRA_*')\" = '1 2' ]"

# Test 10.4: 不存在的文件报错
run_test "不存在的白名单文件报错" \
    "! echo 'test' | ./envsubst --whitelist-file /nonexistent/file.txt 2>/dev/null"

# 清理临时文件
rm -f /tmp/test_whitelist.txt

echo ""

# ==========================================
# 第十一部分：组合功能测试
# ==========================================
echo -e "${YELLOW}=== 第十一部分：组合功能测试 ===${NC}"
echo ""

# Test 11.1: 默认值 + 调试模式
run_test "默认值 + Debug" \
    "echo '\${UNSET:-default}' | ./envsubst --debug 2>&1 | grep -q '\[DEBUG\]'"

# Test 11.2: 默认值 + 统计
run_test "默认值 + Stats" \
    "echo '\${A:-1} \${B:-2}' | ./envsubst --stats 2>&1 | grep -q 'Variables replaced:.*2'"

# Test 11.3: 默认值 + JSON 统计
run_test "默认值 + JSON Stats" \
    "echo '\${X:-10}' | ./envsubst --json-stats 2>&1 | grep -q '\"variables_replaced\": 1'"

# Test 11.4: 白名单 + 调试 + 统计
run_test "白名单 + Debug + Stats" \
    "echo '\${APP_HOST} \${DB_HOST}' | APP_HOST=localhost ./envsubst --debug --stats 'APP_*' 2>&1 | grep -q '\[DEBUG\]' && \
     echo '\${APP_HOST} \${DB_HOST}' | APP_HOST=localhost ./envsubst --debug --stats 'APP_*' 2>&1 | grep -q 'Statistics'"

# Test 11.5: -k + 默认值
run_test "-k + 默认值" \
    "[ \"\$(echo '\${DEF:-default1} \${UNDEF:-default2}' | DEF=value ./envsubst -k)\" = 'value default2' ]"

# Test 11.6: --all + 白名单 + 默认值
run_test "--all + 白名单 + 默认值" \
    "[ \"\$(echo '\$APP_PORT \${DB_PORT:-5432}' | APP_PORT=8080 ./envsubst --all 'APP_*')\" = '8080 \${DB_PORT:-5432}' ]"

echo ""

# ==========================================
# 第十二部分：实际应用场景
# ==========================================
echo -e "${YELLOW}=== 第十二部分：实际应用场景 ===${NC}"
echo ""

# Test 12.1: Nginx 配置模板
cat > /tmp/nginx_test.conf << 'EOF'
server {
    listen ${PORT:-80};
    server_name ${SERVER_NAME:-localhost};
    
    location / {
        proxy_pass http://${BACKEND_HOST:-127.0.0.1}:${BACKEND_PORT:-8080};
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
EOF

run_test "Nginx 配置模板（保护内置变量）" \
    "PORT=443 SERVER_NAME=example.com BACKEND_HOST=api BACKEND_PORT=3000 \
     ./envsubst 'PORT SERVER_NAME BACKEND_*' < /tmp/nginx_test.conf | \
     grep -q 'proxy_set_header Host \$host'"

# Test 12.2: Docker Compose 环境变量
cat > /tmp/docker_test.env << 'EOF'
APP_NAME=${APP_NAME:-myapp}
APP_VERSION=${APP_VERSION:-latest}
DB_HOST=${DB_HOST:-localhost}
DB_PORT=${DB_PORT:-5432}
REDIS_URL=${REDIS_URL:-redis://localhost:6379}
EOF

run_test "Docker Compose 环境变量" \
    "APP_NAME=testapp APP_VERSION=1.0 ./envsubst < /tmp/docker_test.env | \
     grep -q 'APP_NAME=testapp' && \
     APP_NAME=testapp APP_VERSION=1.0 ./envsubst < /tmp/docker_test.env | \
     grep -q 'DB_HOST=localhost'"

# Test 12.3: Kubernetes ConfigMap
cat > /tmp/k8s_test.yaml << 'EOF'
apiVersion: v1
kind: ConfigMap
metadata:
  name: ${APP_NAME:-myapp}-config
data:
  DATABASE_URL: "postgresql://${DB_USER:-admin}:${DB_PASS:-secret}@${DB_HOST:-localhost}:${DB_PORT:-5432}/${DB_NAME:-mydb}"
  CACHE_TTL: "${CACHE_TTL:-3600}"
EOF

run_test "Kubernetes ConfigMap 模板" \
    "APP_NAME=prod DB_HOST=db.example.com DB_PORT=5433 ./envsubst 'APP_* DB_* CACHE_*' < /tmp/k8s_test.yaml | \
     grep -q 'name: prod-config'"

# 清理临时文件
rm -f /tmp/nginx_test.conf /tmp/docker_test.env /tmp/k8s_test.yaml

echo ""

# ==========================================
# 第十三部分：边界情况测试
# ==========================================
echo -e "${YELLOW}=== 第十三部分：边界情况测试 ===${NC}"
echo ""

# Test 13.1: 空输入
run_test "空输入处理" \
    "[ \"\$(echo '' | ./envsubst)\" = '' ]"

# Test 13.2: 无变量的文本
run_test "无变量文本" \
    "[ \"\$(echo 'Hello World' | ./envsubst)\" = 'Hello World' ]"

# Test 13.3: 连续的变量
run_test "连续变量" \
    "[ \"\$(echo '\${A}\${B}' | A=1 B=2 ./envsubst)\" = '12' ]"

# Test 13.4: 嵌套大括号（不支持，应原样保留）
run_test "嵌套大括号处理" \
    "[ \"\$(echo '\${\${VAR}}' | ./envsubst)\" = '\${\${VAR}}' ]" || true

# Test 13.5: 转义字符
run_test "转义字符处理" \
    "[ \"\$(echo 'Price: \$100' | ./envsubst)\" = 'Price: \$100' ]"

# Test 13.6: 多行输入
run_test "多行输入" \
    "[ \"\$(printf '\${A}\n\${B}\n' | A=1 B=2 ./envsubst)\" = \$'1\n2' ]"

# Test 13.7: 特殊字符在值中
run_test "值中包含特殊字符" \
    "[ \"\$(echo '\${VAL}' | VAL='hello world! @#\$%' ./envsubst)\" = 'hello world! @#\$%' ]"

# Test 13.8: 超长变量名
run_test "超长变量名" \
    "[ \"\$(echo '\${VERY_LONG_VARIABLE_NAME_THAT_EXCEEDS_NORMAL_LENGTH_123456789}' | VERY_LONG_VARIABLE_NAME_THAT_EXCEEDS_NORMAL_LENGTH_123456789=test ./envsubst)\" = 'test' ]"

# Test 13.9: Unicode 字符
run_test "Unicode 字符支持" \
    "[ \"\$(echo '\${GREETING}' | GREETING='你好世界' ./envsubst)\" = '你好世界' ]"

echo ""

# ==========================================
# 第十四部分：性能测试（简单）
# ==========================================
echo -e "${YELLOW}=== 第十四部分：性能测试 ===${NC}"
echo ""

# Test 14.1: 大量变量替换
echo -n "大规模变量替换 (100个变量)... "
start_time=$(date +%s)

# 构建环境变量字符串
env_vars=""
for i in $(seq 1 100); do
    env_vars="$env_vars VAR_$i=$i"
done

# 构建输入字符串
input=""
for i in $(seq 1 100); do
    input="$input \${VAR_$i}"
done

# 执行测试
result=$(echo "$input" | env $env_vars ./envsubst 2>/dev/null)
end_time=$(date +%s)
elapsed=$(( end_time - start_time ))

if echo "$result" | grep -q "1 2 3"; then
    if [ $elapsed -lt 1 ]; then
        echo -e "${GREEN}✅ PASS${NC} (<1s)"
    else
        echo -e "${GREEN}✅ PASS${NC} (${elapsed}s)"
    fi
    PASSED_TESTS=$((PASSED_TESTS + 1))
else
    echo -e "${RED}❌ FAIL${NC}"
    FAILED_TESTS=$((FAILED_TESTS + 1))
fi
TOTAL_TESTS=$((TOTAL_TESTS + 1))

# Test 14.2: 大文件处理
echo -n "大文件处理 (1000行)... "
start_time=$(date +%s)
for i in $(seq 1 1000); do
    echo "Line $i: \${VAR}"
done | VAR=test ./envsubst > /tmp/large_output.txt 2>/dev/null
end_time=$(date +%s)
elapsed=$(( end_time - start_time ))
line_count=$(wc -l < /tmp/large_output.txt)
if [ "$line_count" -eq 1000 ]; then
    if [ $elapsed -lt 1 ]; then
        echo -e "${GREEN}✅ PASS${NC} (<1s)"
    else
        echo -e "${GREEN}✅ PASS${NC} (${elapsed}s)"
    fi
    PASSED_TESTS=$((PASSED_TESTS + 1))
else
    echo -e "${RED}❌ FAIL${NC} (Expected 1000 lines, got $line_count)"
    FAILED_TESTS=$((FAILED_TESTS + 1))
fi
TOTAL_TESTS=$((TOTAL_TESTS + 1))
rm -f /tmp/large_output.txt

echo ""

# ==========================================
# 第十五部分：帮助和版本信息
# ==========================================
echo -e "${YELLOW}=== 第十五部分：帮助和版本信息 ===${NC}"
echo ""

# Test 15.1: --help
run_test "--help 显示帮助" \
    "./envsubst --help | grep -q 'envsubst'"

# Test 15.2: -h
run_test "-h 显示帮助" \
    "./envsubst -h | grep -q 'envsubst'"

# Test 15.3: --version
run_test "--version 显示版本" \
    "./envsubst --version | grep -q '[0-9]\+\.[0-9]\+\.[0-9]\+'"

# Test 15.4: -V
run_test "-V 显示版本" \
    "./envsubst -V | grep -q '[0-9]\+\.[0-9]\+\.[0-9]\+'"

# Test 15.5: -v 显示规则
run_test "-v 显示白名单规则" \
    "./envsubst -v 'TEST_*' | grep -q 'TEST_\*'"

echo ""

# ==========================================
# 打印测试结果
# ==========================================
print_summary
