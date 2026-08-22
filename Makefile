# `make status` regenerates Docs/STATE.md. There is no `make` on the Windows
# box's PATH today, so the script is the real entry point and this file is a
# convenience for anyone who has one:
#
#     python Scripts/status.py
#
# Exit codes: 0 clean or unpinned, 1 a pinned section is out of band,
# 2 a source or spec surface stopped matching and the report was refused.

.PHONY: status
status:
	python Scripts/status.py
