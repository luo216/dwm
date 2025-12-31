#!/usr/bin/env bash

# DWM 测试脚本

LOG_FILE="dwm_test.log"
DISPLAY_NUM=":99"

echo "=== DWM 测试开始 ===" | tee "$LOG_FILE"
echo "启动时间: $(date)" | tee -a "$LOG_FILE"

# 检查Xephyr
if ! command -v Xephyr &>/dev/null; then
  echo "错误: 需要安装Xephyr" | tee -a "$LOG_FILE"
  exit 1
fi

# 检查dwm
if [ ! -f "./dwm" ]; then
  echo "编译dwm..." | tee -a "$LOG_FILE"
  make clean && make >> "$LOG_FILE" 2>&1 || exit 1
fi

# 启动虚拟显示
echo "启动虚拟显示..." | tee -a "$LOG_FILE"
Xephyr -ac -br -noreset -screen 1920x1080 $DISPLAY_NUM >/dev/null 2>&1 &
XEPHYR_PID=$!

sleep 1
if ! xdpyinfo -display $DISPLAY_NUM &>/dev/null; then
  echo "错误: 无法启动X服务器" | tee -a "$LOG_FILE"
  kill $XEPHYR_PID 2>/dev/null
  exit 1
fi

echo "✓ 虚拟显示已启动: $DISPLAY_NUM" | tee -a "$LOG_FILE"

# 启动dwm
echo "启动dwm..." | tee -a "$LOG_FILE"
DISPLAY=$DISPLAY_NUM ./dwm >> "$LOG_FILE" 2>&1 &
DWM_PID=$!

echo "✓ DWM已启动 (PID: $DWM_PID)" | tee -a "$LOG_FILE"
echo "按Ctrl+C停止" | tee -a "$LOG_FILE"

# 清理函数
cleanup() {
  echo ""
  echo "清理中..." | tee -a "$LOG_FILE"
  kill $DWM_PID $XEPHYR_PID 2>/dev/null
  echo "结束时间: $(date)" | tee -a "$LOG_FILE"
  exit
}

trap cleanup INT TERM

# 等待dwm
wait $DWM_PID 2>/dev/null
cleanup