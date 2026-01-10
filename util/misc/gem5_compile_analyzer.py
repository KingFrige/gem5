import subprocess
import time
import os
from datetime import timedelta
import shutil
import random

def run_compile(linker, version, lto_enabled, log_file, dry_run):
    """执行或模拟执行gem5编译命令并返回执行时间"""
    # 构建编译命令
    cmd = [
        "scons", "-sQ", f"build/X86/gem5.{version}",
        f"--linker={linker}",
        "-j4"
    ]

    # 如果启用LTO，添加相应参数
    if lto_enabled:
        cmd.append("--with-lto")

    cmd_str = ' '.join(cmd)

    # 记录命令到日志
    with open(log_file, 'a') as f:
        f.write(f"执行命令: {cmd_str}\n")
        f.write("开始编译...\n")
        f.write("="*60 + "\n")
        if dry_run:
            f.write("dry-run模式: 模拟编译输出\n")
        f.write("\n" + "="*60 + "\n")

    # 模拟执行（dry-run模式）
    if dry_run:
        # 生成合理的模拟时间（根据配置类型）
        base_time = 300  # 基础时间（秒）
        if version == "debug":
            base_time *= 1.5  # debug版本通常更慢
        if lto_enabled:
            base_time *= 1.8  # LTO通常增加编译时间
        if linker in ["lld", "mold"]:
            base_time *= 0.7  # 这些链接器通常更快

        # 添加随机波动
        duration = base_time * (0.9 + random.random() * 0.2)
        return_code = 0  # 模拟成功

        # 记录模拟结果到日志
        with open(log_file, 'a') as f:
            f.write(f"dry-run模式: 模拟编译完成\n")
            f.write(f"编译完成，返回代码: {return_code}\n")
            f.write(f"执行时间: {str(timedelta(seconds=duration))}\n\n\n")
        return duration, return_code

    # 实际执行编译并计时
    start_time = time.time()
    result = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True
    )
    end_time = time.time()

    # 计算执行时间
    duration = end_time - start_time

    # 将输出写入日志
    with open(log_file, 'a') as f:
        f.write(result.stdout)
        f.write("\n" + "="*60 + "\n")
        f.write(f"编译完成，返回代码: {result.returncode}\n")
        f.write(f"执行时间: {str(timedelta(seconds=duration))}\n\n\n")

    return duration, result.returncode

def clean_build(dry_run):
    """清理之前的构建文件或模拟清理"""
    build_dir = "build/X86"
    if dry_run:
        print(f"dry-run模式: 模拟清理旧的构建文件: {build_dir}")
        return

    if os.path.exists(build_dir):
        print(f"清理旧的构建文件: {build_dir}")
        shutil.rmtree(build_dir, ignore_errors=True)

def print_single_category_table(results, category_name, fixed_params, fixed_names):
    """打印单一类别的结果表格，高亮显示最快的配置"""
    if not results:
        print("没有结果可显示")
        return

    # 找到所有成功编译中的最短时间
    successful_times = [t for (t, code) in results.values() if code == 0]
    min_time = min(successful_times) if successful_times else None

    # 构建固定参数显示字符串
    fixed_params_str = ", ".join([f"{k}: {v}" for k, v in zip(fixed_names, fixed_params)])

    # 打印表头 - 优化对齐
    print(f"{fixed_params_str}")
    print("\n" + "="*90)
    # print(f"{category_name:<15} | {fixed_params_str:<35} | {'编译时间':<20} | {'时间(秒)':<10} | 状态")
    print(f"{category_name:<15} | {'编译时间':<20} | {'时间(秒)':<10} | 状态")
    print("-"*90)

    # 打印结果 - 优化对齐
    for item, (duration, return_code) in results.items():
        time_str = str(timedelta(seconds=duration))
        status = "成功" if return_code == 0 else "失败"
        status_color = "\033[92m成功\033[0m" if return_code == 0 else "\033[91m失败\033[0m"

        # 高亮显示最短时间
        if min_time is not None and duration == min_time and return_code == 0:
            # print(f"\033[1;93m{item:<15} | {'':<35} | {time_str:<20} | {duration:>8.2f}s  | {status_color}\033[0m")
            print(f"\033[1;93m{item:<15} | {time_str:<20} | {duration:>8.2f}s  | {status_color}\033[0m")
        else:
            # print(f"{item:<15} | {'':<35} | {time_str:<20} | {duration:>8.2f}s  | {status_color}")
            print(f"{item:<15} | {time_str:<20} | {duration:>8.2f}s  | {status_color}")

    print("="*90 + "\n")

def run_linker_test(fixed_version, log_file, linkers, dry_run):
    """测试不同链接器，使用固定版本和禁用LTO"""
    print(f"\n===== 开始测试不同链接器 (固定版本: {fixed_version}, LTO: 禁用) =====")
    results = {}

    for i, linker in enumerate(linkers, 1):
        print(f"({i}/{len(linkers)}) 正在使用 {linker} 链接器编译 gem5.{fixed_version}...")

        clean_build(dry_run)
        duration, return_code = run_compile(linker, fixed_version, False, log_file, dry_run)
        results[linker] = (duration, return_code)

        status = "成功" if return_code == 0 else "失败"
        print(f"{linker} 编译{status}，耗时: {str(timedelta(seconds=duration))}\n")

    print("\n===== 链接器测试结果 =====")
    print_single_category_table(results, "链接器", [fixed_version, "禁用"], ["固定版本", "LTO"])
    return results

def run_version_test(fixed_linker, log_file, versions, dry_run):
    """测试不同版本，使用固定链接器和禁用LTO"""
    print(f"\n===== 开始测试不同版本 (固定链接器: {fixed_linker}, LTO: 禁用) =====")
    results = {}

    for i, version in enumerate(versions, 1):
        print(f"({i}/{len(versions)}) 正在使用 {fixed_linker} 链接器编译 gem5.{version}...")

        clean_build(dry_run)
        duration, return_code = run_compile(fixed_linker, version, False, log_file, dry_run)
        results[version] = (duration, return_code)

        status = "成功" if return_code == 0 else "失败"
        print(f"{version} 编译{status}，耗时: {str(timedelta(seconds=duration))}\n")

    print("\n===== 版本测试结果 =====")
    print_single_category_table(results, "版本", [fixed_linker, "禁用"], ["固定链接器", "LTO"])
    return results

def run_lto_test(fixed_linker, fixed_version, log_file, dry_run):
    """测试LTO开关对比，使用固定链接器和版本"""
    print(f"\n===== 开始测试LTO开关 (固定链接器: {fixed_linker}, 固定版本: {fixed_version}) =====")
    results = {
        "禁用LTO": None,
        "启用LTO": None
    }

    # 测试禁用LTO
    print(f"(1/2) 测试禁用LTO...")
    clean_build(dry_run)
    duration, return_code = run_compile(fixed_linker, fixed_version, False, log_file, dry_run)
    results["禁用LTO"] = (duration, return_code)
    status = "成功" if return_code == 0 else "失败"
    print(f"禁用LTO 编译{status}，耗时: {str(timedelta(seconds=duration))}\n")

    # 测试启用LTO
    print(f"(2/2) 测试启用LTO...")
    clean_build(dry_run)
    duration, return_code = run_compile(fixed_linker, fixed_version, True, log_file, dry_run)
    results["启用LTO"] = (duration, return_code)
    status = "成功" if return_code == 0 else "失败"
    print(f"启用LTO 编译{status}，耗时: {str(timedelta(seconds=duration))}\n")

    print("\n===== LTO测试结果 =====")
    print_single_category_table(results, "LTO配置", [fixed_linker, fixed_version], ["固定链接器", "固定版本"])
    return results

def main():
    import argparse

    # 解析命令行参数，默认启用dry-run
    parser = argparse.ArgumentParser(description='gem5编译分析工具')
    parser.add_argument('--no-dry-run', action='store_true', help='禁用dry-run模式，实际执行编译命令')
    args = parser.parse_args()

    # 确定是否启用dry-run（默认启用）
    dry_run = not args.no_dry_run

    # 配置测试参数 - 将fd修正为bfd
    linkers = ["bfd", "gold", "lld", "mold"]
    versions = ["opt", "debug", "fast"]
    fixed_version_for_linker_test = "opt"       # 测试链接器时使用的固定版本
    fixed_linker_for_version_test = "gold"      # 测试版本时使用的固定链接器
    fixed_linker_for_lto_test = "gold"          # 测试LTO时使用的固定链接器
    fixed_version_for_lto_test = "opt"          # 测试LTO时使用的固定版本

    # 日志文件路径
    log_file = "gem5_compile_log.txt"

    print("gem5编译分析工具")
    print("="*60)
    if dry_run:
        print("\033[1;94m默认运行在dry-run模式下 - 不会实际执行编译命令\033[0m")
        print("使用 --no-dry-run 参数执行实际编译测试")
    print("测试分为三部分:")
    print(f"1. 测试不同链接器 (固定版本: {fixed_version_for_linker_test}, LTO: 禁用)")
    print(f"2. 测试不同版本 (固定链接器: {fixed_linker_for_version_test}, LTO: 禁用)")
    print(f"3. 测试LTO开关 (固定链接器: {fixed_linker_for_lto_test}, 固定版本: {fixed_version_for_lto_test})")
    print(f"所有输出将记录到: {log_file}")
    print("="*60 + "\n")

    # 清除日志文件
    if os.path.exists(log_file):
        os.remove(log_file)

    # 第一部分：测试不同链接器（固定版本，禁用LTO）
    linker_results = run_linker_test(fixed_version_for_linker_test, log_file, linkers, dry_run)

    # 第二部分：测试不同版本（固定链接器，禁用LTO）
    version_results = run_version_test(fixed_linker_for_version_test, log_file, versions, dry_run)

    # 第三部分：测试LTO开关（固定链接器和版本）
    lto_results = run_lto_test(fixed_linker_for_lto_test, fixed_version_for_lto_test, log_file, dry_run)

    # 总结
    print("\n" + "="*60)
    print("          所有测试完成")
    print("="*60)
    print("所有测试结果已记录到日志文件")
    print("\033[1;93m黄色高亮\033[0m 表示同组测试中最快的配置")
    print("\033[92m绿色\033[0m 表示编译成功，\033[91m红色\033[0m 表示编译失败\n")
    if dry_run:
        print("\033[1;94m提示: 这是dry-run模式的模拟结果，使用 --no-dry-run 获得实际结果\033[0m")

if __name__ == "__main__":
    main()

