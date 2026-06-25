@echo off
chcp 65001 >nul
echo ===================== Git一键提交脚本 =====================
set /p msg=请输入本次提交备注：
git add .
git status
git commit -m "%msg%"
git push
echo.
echo 执行完成，按任意键关闭窗口
pause