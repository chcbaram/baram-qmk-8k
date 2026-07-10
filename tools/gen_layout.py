#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
VIA JSON 의 KLE 레이아웃을 파싱해 펌웨어에 임베드할 바이너리 레이아웃을 생성한다.

각 보드의 keyboards/geon/<board>/port/layout_def.c 를 만들고, 여기에
boardLayoutGet() (web_hid.c 의 weak 심볼 override)이 키 배열을 반환하도록 한다.

키 1개 = 6바이트: x, y, w, h (1/4 키유닛), row, col.
웹 대시보드가 이 데이터를 raw HID 로 받아 물리 키 배열을 그린다.

사용법:
    python3 tools/gen_layout.py               # VENOM 4종 전체 생성 (수동)
    python3 tools/gen_layout.py --dir <path>  # 지정 보드 폴더 하나만 생성 (빌드 자동호출용)

빌드 시 CMake(configure) 단계에서 --dir 로 현재 보드에 대해 자동 호출된다.
"""

import glob
import json
import os
import sys

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEON_DIR = os.path.join(
    ROOT_DIR, "src", "ap", "modules", "qmk", "keyboards", "geon")

VENOM_KEYBOARDS = [
    "VENOM-60MX-6.25U",
    "VENOM-60MX-7U",
    "VENOM-80MX-6.25U",
    "VENOM-80MX-7U",
]


def parse_kle(rows):
    """KLE keymap 을 표준 알고리즘으로 파싱해 키 목록을 반환한다.

    각 키: (x, y, w, h, row, col)  (단위: 키유닛, float)
    키의 매트릭스 위치는 라벨 첫 줄의 "row,col" 에서 얻는다.
    """
    keys = []
    y = 0.0
    for row in rows:
        x = 0.0
        w = h = 1.0
        for item in row:
            if isinstance(item, dict):
                x += item.get("x", 0)
                y += item.get("y", 0)
                if "w" in item:
                    w = item["w"]
                if "h" in item:
                    h = item["h"]
                # w2/h2/rotation/stepped 등은 테스터에선 무시(사각형 근사)
            else:
                label = str(item).split("\n")[0]
                r, c = label.split(",")
                keys.append((x, y, w, h, int(r), int(c)))
                x += w
                w = h = 1.0
        y += 1.0
    return keys


def q(v):
    """키유닛 -> 1/4 유닛 정수 (0..255 클램프)."""
    n = int(round(v * 4))
    return max(0, min(255, n))


def find_via_json(board_dir):
    """보드 폴더의 json/ 에서 *-VIA.JSON 파일 경로를 찾는다. 없으면 None."""
    matches = glob.glob(os.path.join(board_dir, "json", "*-VIA.JSON"))
    matches += glob.glob(os.path.join(board_dir, "json", "*-VIA.json"))
    return matches[0] if matches else None


def gen_board_dir(board_dir):
    """보드 폴더 하나에 대해 port/layout_def.c 를 생성한다. VIA JSON 없으면 skip."""
    name = os.path.basename(os.path.normpath(board_dir))
    via_path = find_via_json(board_dir)
    if via_path is None:
        print("  {:20s} : VIA JSON 없음 - skip".format(name))
        return

    with open(via_path, "r", encoding="utf-8") as f:
        d = json.load(f)
    if "layouts" not in d or "keymap" not in d["layouts"]:
        print("  {:20s} : keymap 없음 - skip".format(name))
        return

    keys = parse_kle(d["layouts"]["keymap"])

    lines = []
    lines.append("// 자동 생성 파일 - 수정하지 말 것 (gitignore 대상).")
    lines.append("// 생성: tools/gen_layout.py  (원본: {})".format(
        os.path.basename(via_path)))
    lines.append("// 각 키 6바이트: x, y, w, h (1/4 키유닛), row, col")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("static const uint8_t layout_data[] = {")
    for (x, y, w, h, r, c) in keys:
        lines.append("  {:3d},{:3d},{:3d},{:3d},{:3d},{:3d},".format(
            q(x), q(y), q(w), q(h), r, c))
    lines.append("};")
    lines.append("")
    lines.append("uint16_t boardLayoutGet(const uint8_t **pp)")
    lines.append("{")
    lines.append("  *pp = layout_data;")
    lines.append("  return sizeof(layout_data);")
    lines.append("}")
    lines.append("")

    out_dir = os.path.join(board_dir, "port")
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "layout_def.c")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

    print("  {:20s} : {} keys -> {}".format(
        name, len(keys), os.path.relpath(out_path, ROOT_DIR)))


def main():
    args = sys.argv[1:]

    if len(args) >= 2 and args[0] == "--dir":
        # 빌드(CMake configure)에서 현재 보드 하나만 생성
        gen_board_dir(args[1])
        return

    # 인자 없음: VENOM 4종 전체 생성 (수동)
    print("레이아웃 생성:")
    for kb in VENOM_KEYBOARDS:
        gen_board_dir(os.path.join(GEON_DIR, kb))


if __name__ == "__main__":
    main()
