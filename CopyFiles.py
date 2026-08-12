# ==============================================================================
# CopyFiles.py — 头文件分发：递归拷贝 Modules/ 下所有 .h/.hpp 到 dist/include/
#   规则：src/ 目录内的头文件 = 实现内部，不对外 → 不拷贝；
#         其余（模块根、子目录）的 .h/.hpp 全部拷贝，保证 include 依赖链完整。
#   保留模块目录结构：Modules/<Name>/.../x.h → dist/include/<Name>/.../x.h
#   lib 产物不在这里管（由 CMake ARCHIVE_OUTPUT_DIRECTORY 直接输出到 dist/lib/）
# 用法：python CopyFiles.py
# ==============================================================================
import shutil
from pathlib import Path

root = Path(__file__).parent
modules = root / "Modules"
dst = root / "dist" / "include"

# 清掉旧的 include，避免残留
if dst.exists():
    shutil.rmtree(dst)
dst.mkdir(parents=True)

count = 0
for mod in modules.iterdir():
    if not mod.is_dir():
        continue
    for f in mod.rglob("*"):
        # 只处理头文件，且跳过 src/ 目录（实现内部不对外）
        if f.suffix.lower() not in (".h", ".hpp"):
            continue
        if "src" in f.parts:
            continue
        # 相对 Modules/ 的路径（去掉 Modules/ 前缀，以模块名开头）
        rel = f.relative_to(modules)
        out = dst / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(f, out)
        count += 1

print(f"[完成] 已拷贝 {count} 个头文件到 {dst}（已跳过各模块 src/ 内的实现头）")
