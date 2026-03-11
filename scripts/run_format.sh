find . -regextype egrep -regex ".*\.(c|cc|cpp|h|hh|hpp)$" -not -path '*/install/*' \
  -not -path '*/build/*' -not -path '*/log/*' -not -path '*/deps/*' \
  -not -path '*/third_party/*' -not -path '*/anti/*' | xargs clang-format -i
