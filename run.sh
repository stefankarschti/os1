#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/build}"
TOOLCHAIN_FILE="${TOOLCHAIN_FILE:-$SCRIPT_DIR/cmake/toolchains/x86_64-elf.cmake}"

TARGET_MENU_ITEMS=$(cat <<'EOF'
run|Default modern UEFI path|Boot OVMF on q35 with the generated virtio-blk test disk used by the modern-platform smoke.
run_serial|Serial-first UEFI shell session|Boot the same OVMF + ISO path, route the guest serial console to this terminal, and disable graphics.
run_bios|Legacy BIOS compatibility path|Boot the raw BIOS image on q35 with the same secondary virtio-blk test disk attached.
run_bios_serial|Serial-first BIOS shell session|Keep the BIOS boot path but route the shell through serial stdio instead of the display-first run target.
smoke|Modern UEFI shell baseline smoke|Run the baseline UEFI shell smoke test.
smoke_observe|Modern UEFI observability smoke|Run the UEFI observability smoke that exercises shell inspection commands.
smoke_balance|Modern UEFI SMP balance smoke|Run the UEFI SMP balance smoke.
smoke_spawn|Modern UEFI child-launch smoke|Run the UEFI child-process spawn smoke.
smoke_exec|Modern UEFI exec smoke|Run the UEFI exec smoke.
smoke_xhci|Modern UEFI xHCI + USB-keyboard smoke|Run the UEFI-only xHCI and USB-keyboard smoke.
smoke_bios|Legacy BIOS shell baseline smoke|Run the baseline BIOS shell smoke test.
smoke_observe_bios|Legacy BIOS observability smoke|Run the BIOS observability smoke that exercises shell inspection commands.
smoke_balance_bios|Legacy BIOS SMP balance smoke|Run the BIOS SMP balance smoke.
smoke_spawn_bios|Legacy BIOS child-launch smoke|Run the BIOS child-process spawn smoke.
smoke_exec_bios|Legacy BIOS exec smoke|Run the BIOS exec smoke.
smoke_all|Full smoke matrix|Run the full smoke matrix across the configured boot paths.
EOF
)
TARGET_MENU_COUNT=$(printf '%s\n' "$TARGET_MENU_ITEMS" | wc -l | tr -d '[:space:]')

menu_cleanup() {
	if [ -n "${MENU_STTY-}" ]; then
		stty "$MENU_STTY"
		MENU_STTY=
	fi
	printf '\033[0m\033[?25h' >&2
}

menu_record() {
	printf '%s\n' "$TARGET_MENU_ITEMS" | sed -n "${1}p"
}

draw_target_menu() {
	selection=$1
	selected_record=$(menu_record "$selection")
	selected_rest=${selected_record#*|}
	selected_label=${selected_rest%%|*}
	selected_description=${selected_rest#*|}

	printf '\033[2J\033[H' >&2
	printf 'Select an os1 target\n' >&2
	printf 'Use Up/Down to move, Enter to run, q to cancel.\n\n' >&2

	i=1
	while [ "$i" -le "$TARGET_MENU_COUNT" ]; do
		record=$(menu_record "$i")
		target_name=${record%%|*}
		record_rest=${record#*|}
		target_label=${record_rest%%|*}
		if [ "$i" -eq "$selection" ]; then
			printf '\033[7m> %-20s %s\033[0m\n' "$target_name" "$target_label" >&2
		else
			printf '  %-20s %s\n' "$target_name" "$target_label" >&2
		fi
		i=$((i + 1))
	done

	printf '\nDescription\n' >&2
	printf '  %s\n' "$selected_label" >&2
	printf '  %s\n' "$selected_description" >&2
}

read_menu_key() {
	key=$(dd bs=1 count=1 2>/dev/null || true)
	if [ "$key" = "$(printf '\033')" ]; then
		key="$key$(dd bs=1 count=1 2>/dev/null || true)$(dd bs=1 count=1 2>/dev/null || true)"
	fi
	printf '%s' "$key"
}

choose_target() {
	selection=1
	MENU_STTY=$(stty -g)
	trap 'menu_cleanup; printf "\n"; exit 130' INT TERM HUP

	stty -echo -icanon min 1 time 0
	printf '\033[?25l' >&2

	while :; do
		draw_target_menu "$selection"
		key=$(read_menu_key)
		case "$key" in
			"$(printf '\033[A')")
				selection=$((selection - 1))
				if [ "$selection" -lt 1 ]; then
					selection=$TARGET_MENU_COUNT
				fi
				;;
			"$(printf '\033[B')")
				selection=$((selection + 1))
				if [ "$selection" -gt "$TARGET_MENU_COUNT" ]; then
					selection=1
				fi
				;;
			"$(printf '\n')"|"$(printf '\r')")
				break
				;;
			q|Q)
				menu_cleanup
				trap - INT TERM HUP
				printf '\n' >&2
				exit 130
				;;
		esac
	done

	menu_cleanup
	trap - INT TERM HUP
	printf '\033[2J\033[H' >&2
	record=$(menu_record "$selection")
	printf '%s\n' "${record%%|*}"
}

if [ "$#" -gt 0 ]; then
	TARGET=$1
elif [ -t 0 ] && [ -t 1 ]; then
	TARGET=$(choose_target)
	printf 'Selected target: %s\n' "$TARGET"
else
	TARGET=run
	printf 'No target specified and no interactive terminal is available; defaulting to %s.\n' "$TARGET" >&2
fi

cmake --preset default
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE"
cmake --build "$BUILD_DIR"
cmake --build "$BUILD_DIR" --target "$TARGET"
