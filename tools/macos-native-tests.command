#!/bin/sh
# Run the Mac-built binaries without reading or writing user settings.
looklan_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)" || exit 1
looklan_temp="$(mktemp -d -t looklan-validation)" || exit 1
trap 'rm -rf -- "$looklan_temp"' EXIT HUP INT TERM
cd "$looklan_temp" || exit 1
looklan_log="$looklan_dir/Native-tests.log"
(
    printf 'macOS native validation\n'
    /usr/bin/sw_vers
    /usr/bin/uname -m
    looklan_result=0
    for looklan_test in core_tests feature_tests monitor_tests profile_report_tests details_nbns_tests posix_runtime_tests; do
        printf '\n--- %s ---\n' "$looklan_test"
        env -u HOME "$looklan_dir/Native-validation/$looklan_test"
        looklan_status=$?
        printf 'Exit: %s\n' "$looklan_status"
        if [ "$looklan_status" -eq 77 ]; then
            [ "$looklan_result" -eq 0 ] && looklan_result=77
        elif [ "$looklan_status" -ne 0 ]; then
            looklan_result=1
        fi
    done
    printf '\nNative result: %s (0=passed, 77=skipped, 1=failed)\n' "$looklan_result"
    exit "$looklan_result"
) >"$looklan_log" 2>&1
looklan_result=$?
cat "$looklan_log"
exit "$looklan_result"
