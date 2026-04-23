#!/bin/sh
# OpenResty entrypoint script with dynamic WAF cache configuration

set -e

rm -f test_data/nginx.conf

# 设置默认值并导出为环境变量
export WAF_CACHE_SIZE=${WAF_CACHE_SIZE:-5m}
export WAF_API_LISTEN=${WAF_API_LISTEN:-127.0.0.1:8081}
export WAF_API_SERVER_NAME=${WAF_API_SERVER_NAME:-localhost}

export REST_BROTLI_STATUS=${REST_BROTLI_STATUS:-on}
export REST_BROTLI_COMP_LEVEL=${REST_BROTLI_COMP_LEVEL:-6}
export REST_GZIP_STATUS=${REST_GZIP_STATUS:-on}


export MYSQL_ENABLE_BINLOG=${MYSQL_ENABLE_BINLOG:-on}
export MYSQL_LOG_BIN=${MYSQL_LOG_BIN:-diy-bin}


ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE_CONF="${ROOT_DIR}/test_data/nginx.conf.template"
TARGET_CONF="${ROOT_DIR}/test_data/nginx.conf"

if [ -f "$TEMPLATE_CONF" ]; then
    echo "Generating nginx.conf..."
    
    ${ROOT_DIR}/envsubst  < "$TEMPLATE_CONF" > "$TARGET_CONF"
    
    echo "Configuration generated."
fi


# Execute the main process (CMD 里的内容)
exec "$@"