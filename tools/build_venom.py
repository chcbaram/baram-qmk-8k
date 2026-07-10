#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
VENOM 프로젝트 4종을 일괄 빌드하는 스크립트.

각 키보드마다 별도의 build 디렉토리를 사용해서 서로 간섭 없이 빌드하고,
결과물을 빌드 날짜 + 리비전 기반 통합 폴더로 모은다.

통합 폴더 이름은 output/VENOM-V<YYMMDD>R<n> 형태이며, 같은 날 다시 빌드하면
리비전(R1, R2, ...)이 자동으로 증가한다. (--rev 로 직접 지정 가능)

통합 폴더 아래에는 보드별 하위 폴더가 생기고, 각 폴더에는 uf2 펌웨어와
대응하는 VIA JSON 파일이 함께 들어간다.

    output/VENOM-V260710R1/
      VENOM-60MX-6.25U/
        VENOM-60MX-6.25U-V250119R1.uf2
        VENOM-60MX-6.25U-VIA.JSON
      VENOM-60MX-7U/
        ...

사용법:
    python3 tools/build_venom.py            # 전체 빌드 (리비전 자동 증가)
    python3 tools/build_venom.py -c         # 빌드 디렉토리 정리 후 빌드 (clean)
    python3 tools/build_venom.py VENOM-60MX-7U   # 특정 키보드만 빌드
    python3 tools/build_venom.py -j 8       # 병렬 job 수 지정
    python3 tools/build_venom.py --rev 3    # 리비전 번호 직접 지정 (R3)
"""

import argparse
import datetime
import os
import re
import shutil
import subprocess
import sys

# tools/ 의 상위 = 프로젝트 루트
ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# QMK 키보드 경로 기준 디렉토리 (config.cmake 의 KEYBOARD_PATH 기준과 동일)
KEYBOARD_BASE = "/keyboards/geon"

# 키보드 소스 폴더 (VIA JSON 등을 찾는 기준)
KEYBOARD_SRC_DIR = os.path.join(
    ROOT_DIR, "src", "ap", "modules", "qmk", "keyboards", "geon")

# 빌드 대상 VENOM 프로젝트 4종
VENOM_KEYBOARDS = [
    "VENOM-60MX-6.25U",
    "VENOM-60MX-7U",
    "VENOM-80MX-6.25U",
    "VENOM-80MX-7U",
]

OUTPUT_BASE_DIR = os.path.join(ROOT_DIR, "output")


def resolve_output_dir(rev):
    """빌드 날짜 + 리비전 기반 통합 폴더 경로를 반환한다.

    rev 가 None 이면 같은 날짜의 기존 폴더를 조사해 다음 리비전을 자동으로 부여한다.
    (예: output/VENOM-V260710R1)
    """
    date_tag = datetime.date.today().strftime("V%y%m%d")   # 예: V260710
    prefix = "VENOM-" + date_tag + "R"

    if rev is None:
        max_rev = 0
        if os.path.isdir(OUTPUT_BASE_DIR):
            for name in os.listdir(OUTPUT_BASE_DIR):
                m = re.match("^" + re.escape(prefix) + r"(\d+)$", name)
                if m:
                    max_rev = max(max_rev, int(m.group(1)))
        rev = max_rev + 1

    return os.path.join(OUTPUT_BASE_DIR, prefix + str(rev))


def find_via_json(keyboard):
    """키보드에 대응하는 VIA JSON 파일 경로를 반환한다. 없으면 None."""
    path = os.path.join(
        KEYBOARD_SRC_DIR, keyboard, "json", keyboard + "-VIA.JSON")
    return path if os.path.isfile(path) else None


def run(cmd):
    """명령을 실행하고 실패 시 예외를 발생시킨다."""
    print("  $ " + " ".join(cmd))
    subprocess.run(cmd, cwd=ROOT_DIR, check=True)


def build_one(keyboard, jobs, clean):
    """단일 VENOM 키보드를 빌드하고 생성된 uf2 경로 목록을 반환한다."""
    keyboard_path = KEYBOARD_BASE + "/" + keyboard
    build_dir = os.path.join(ROOT_DIR, "build-" + keyboard)

    print("=" * 70)
    print("[BUILD] {}".format(keyboard))
    print("=" * 70)

    if clean and os.path.isdir(build_dir):
        print("  clean: remove {}".format(build_dir))
        shutil.rmtree(build_dir)

    # configure
    run(["cmake", "-S", ".", "-B", build_dir,
         "-DKEYBOARD_PATH=" + keyboard_path])
    # build
    run(["cmake", "--build", build_dir, "-j", str(jobs)])

    # 생성된 uf2 수집
    uf2_files = []
    for name in os.listdir(build_dir):
        if name.endswith(".uf2"):
            uf2_files.append(os.path.join(build_dir, name))
    return uf2_files


def main():
    parser = argparse.ArgumentParser(
        description="VENOM 프로젝트 일괄 빌드 스크립트")
    parser.add_argument("keyboards", nargs="*",
                        help="빌드할 키보드 이름 (미지정 시 전체 빌드)")
    parser.add_argument("-c", "--clean", action="store_true",
                        help="빌드 디렉토리를 삭제하고 새로 빌드")
    parser.add_argument("-j", "--jobs", type=int, default=10,
                        help="병렬 빌드 job 수 (기본: 10)")
    parser.add_argument("-r", "--rev", type=int, default=None,
                        help="통합 폴더 리비전 번호 직접 지정 "
                             "(미지정 시 자동 증가)")
    parser.add_argument("--no-zip", action="store_true",
                        help="릴리즈용 zip 생성 안 함")
    args = parser.parse_args()

    targets = args.keyboards if args.keyboards else VENOM_KEYBOARDS

    # 잘못된 키보드 이름 검증
    unknown = [k for k in targets if k not in VENOM_KEYBOARDS]
    if unknown:
        print("알 수 없는 키보드: {}".format(", ".join(unknown)))
        print("사용 가능: {}".format(", ".join(VENOM_KEYBOARDS)))
        return 1

    # 빌드 날짜 + 리비전 기반 통합 폴더 (예: output/VENOM-V260710R1)
    output_dir = resolve_output_dir(args.rev)
    os.makedirs(output_dir, exist_ok=True)
    print("통합 폴더: {}".format(output_dir))

    results = {}   # keyboard -> "OK" / "FAIL"
    collected = []
    for keyboard in targets:
        try:
            uf2_files = build_one(keyboard, args.jobs, args.clean)

            # 통합 폴더 아래 보드별 하위 폴더 생성
            board_dir = os.path.join(output_dir, keyboard)
            os.makedirs(board_dir, exist_ok=True)

            # uf2 펌웨어 복사
            for src in uf2_files:
                dst = os.path.join(board_dir, os.path.basename(src))
                shutil.copy2(src, dst)
                collected.append(dst)

            # 대응하는 VIA JSON 복사
            via_json = find_via_json(keyboard)
            if via_json:
                dst = os.path.join(board_dir, os.path.basename(via_json))
                shutil.copy2(via_json, dst)
                collected.append(dst)
            else:
                print("  [경고] VIA JSON 을 찾을 수 없음: {}".format(keyboard))

            results[keyboard] = "OK"
        except subprocess.CalledProcessError:
            results[keyboard] = "FAIL"

    # 결과 요약
    print()
    print("=" * 70)
    print("빌드 결과")
    print("=" * 70)
    for keyboard in targets:
        print("  {:20s} : {}".format(keyboard, results.get(keyboard, "?")))
    print()
    print("생성 파일 ({} 개) -> {}".format(len(collected), output_dir))
    for path in collected:
        rel = os.path.relpath(path, output_dir)
        print("  - " + rel)

    all_ok = all(v == "OK" for v in results.values())

    # 릴리즈용 zip 생성 (GitHub Release 에 업로드 -> 웹 대시보드 '펌웨어' 탭에서 다운로드)
    # 통째로 빌드해 전부 성공했을 때만 만든다.
    if all_ok and not args.no_zip and set(targets) == set(VENOM_KEYBOARDS):
        zip_path = shutil.make_archive(output_dir, "zip",
                                       root_dir=OUTPUT_BASE_DIR,
                                       base_dir=os.path.basename(output_dir))
        print()
        print("릴리즈 zip -> {}".format(os.path.relpath(zip_path, ROOT_DIR)))

    # 하나라도 실패하면 비정상 종료
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
