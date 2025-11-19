#!/bin/bash

# 定义帮助信息
usage() {
    echo "用法: $0 <版本号>"
    echo "示例: $0 v1.2.3   # 对比指定版本与其下一个版本"
    exit 1
}

# 仅支持1个参数（指定版本号）
if [ $# -ne 1 ]; then
    echo "错误: 必须传入1个版本号参数"
    usage
fi
target_version="$1"

# 检查是否在git仓库中
if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "错误: 当前目录不在git仓库中"
    exit 1
fi

# 检查暂存区是否干净（无未提交修改）
if ! git diff --cached --quiet; then
    echo "错误: 暂存区存在未提交修改，请先提交或 stash 后再执行"
    exit 1
fi

# 验证目标版本存在
if ! git rev-parse "${target_version}^{commit}" >/dev/null 2>&1; then
    echo "错误: 版本 ${target_version} 不存在"
    exit 1
fi

# 保存当前分支
original_branch=$(git rev-parse --abbrev-ref HEAD)
echo "已记录原始分支: $original_branch"

# 查找目标版本的下一个版本
next_version=$(git log --pretty=format:%H ${target_version}.. --reverse | head -n 1)
if [ -z "$next_version" ]; then
    echo "错误: 版本 ${target_version} 没有后续版本"
    exit 1
fi

# 保存工作区未暂存修改（若有）
echo "正在保存未暂存修改..."
git stash save --quiet "版本切换临时备份"
stash_needed=$?

# 定义在指定版本执行命令的函数
run_in_version() {
    local version=$1
    local version_name=$2

    echo -e "\n===== 切换到${version_name}版本 ====="
    echo "版本哈希: $(git rev-parse --short "${version}")"

    # 切换版本
    if ! git checkout --force "${version}" >/dev/null 2>&1; then
        echo "切换到${version_name}版本失败"
        return 1
    fi

    # 执行命令（替换为实际需要执行的命令）
    echo -e "\n----- 在${version_name}版本中执行命令 -----"
    echo "当前版本信息: $(git log -1 --oneline)"
    echo "文件总数: $(find . -type f ! -path "./.git/*" | wc -l)"
    # ./your_script.sh  # 实际命令
}

# 执行目标版本
run_in_version "${target_version}" "目标"

# 执行下一个版本
run_in_version "${next_version}" "下一个"

# 恢复原始分支
echo -e "\n===== 恢复工作区 ====="
git checkout --force "${original_branch}" >/dev/null 2>&1
echo "已恢复到原始分支: ${original_branch}"

# 恢复未暂存修改
if [ ${stash_needed} -eq 0 ] && git stash list --quiet | grep -q "版本切换临时备份"; then
    git stash pop --quiet stash@{0}
    echo "已恢复未暂存修改"
else
    echo "无未暂存修改需要恢复"
fi

echo -e "\n操作完成"
