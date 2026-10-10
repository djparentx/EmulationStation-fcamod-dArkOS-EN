#!/bin/bash
# Phase 1 of the AmberELEC theme-engine port: fetch MathExpr + HtmlColor verbatim from
# AmberELEC/emulationstation (pinned commit) into es-core/src/utils/, then apply the
# two local adaptations (EsLocale.h, Settings::getBool). Run from the repo root folder
# this script sits in. No arguments.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMMIT="efa3f07710bf8f01d2957737f0b71df48f1b8bc4"
BASE="https://raw.githubusercontent.com/AmberELEC/emulationstation/${COMMIT}/es-core/src/utils"
DEST="${ROOT}/es-core/src/utils"

[ -d "${DEST}" ] || { echo "not an ES repo root: ${ROOT}"; exit 1; }

for f in MathExpr.h MathExpr.cpp HtmlColor.h HtmlColor.cpp; do
	curl -sSfL -o "${DEST}/${f}" "${BASE}/${f}"
	printf "%-14s %s\n" "fetched" "es-core/src/utils/${f}"
done

sed -i \
	-e 's|#include "LocaleES.h"|#include "EsLocale.h"|' \
	-e 's|Settings::ClockMode12()|Settings::getInstance()->getBool("ClockMode12")|g' \
	"${DEST}/MathExpr.cpp"
printf "%-14s %s\n" "adapted" "es-core/src/utils/MathExpr.cpp"

rm -f "${DEST}/ThemeExpr.h" "${DEST}/ThemeExpr.cpp"
printf "%-14s %s\n" "removed" "es-core/src/utils/ThemeExpr.h, ThemeExpr.cpp"