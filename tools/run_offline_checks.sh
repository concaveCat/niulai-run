#!/bin/sh
# Run the repository checks that do not require an ESP32, phone or actuator.
set -eu

REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$REPO_ROOT"

echo "== firmware Python tests =="
python3 -m unittest discover -s firmware/tests -p 'test_*.py' -v

echo "== tool tests =="
python3 -m unittest discover -s tools/tests -p 'test_*.py' -v

echo "== feedback proxy tests =="
python3 -m unittest discover -s services/feedback_proxy/tests -p 'test_*.py' -v

echo "== active Carrier paper checks =="
python3 hardware/electronics/calculations/verify_modular_carrier_interface.py
python3 hardware/electronics/calculations/verify_modular_carrier_power_protection.py

echo "== ESP32 pure-C host tests =="
(cd firmware-esp32/tests/host && ./run_host_tests.sh)

echo "== isolated servo-zero us-profile host test =="
(cd firmware-esp32/tools/servo-zero-test && \
    cc -std=c11 -Wall -Wextra -Werror \
       main/servo_zero_us_profile.c tests/test_servo_zero_us_profile.c \
       -o /tmp/niulai-servo-zero-us-profile-test && \
    /tmp/niulai-servo-zero-us-profile-test)

echo "== isolated servo-zero four-leg foot-space demo host test =="
(cd firmware-esp32/tools/servo-zero-test && \
    cc -std=c11 -Wall -Wextra -Werror -pedantic -I main \
       main/servo_zero_leg_math.c main/servo_zero_quad_demo.c \
       tests/test_servo_zero_quad_demo.c -lm \
       -o /tmp/niulai-servo-zero-quad-demo-test && \
    /tmp/niulai-servo-zero-quad-demo-test)

echo "== documentation audits =="
python3 tools/check_markdown_links.py
python3 tools/check_experiment_index.py
python3 tools/check_decision_index.py

echo "== whitespace check =="
if command -v git >/dev/null 2>&1 && git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git diff --check
else
    echo "git worktree unavailable; skipping git diff --check"
fi

if [ "${NIULAI_RUN_ANDROID:-0}" = "1" ]; then
    echo "== Android unit tests =="
    (cd android-controller && ./gradlew testDebugUnitTest)
else
    echo "Android unit tests: skipped (set NIULAI_RUN_ANDROID=1 to run)"
fi

echo "ALL OFFLINE CHECKS PASSED"
