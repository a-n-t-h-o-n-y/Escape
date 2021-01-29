#!/usr/bin/bash

# Emits certain information about each entry in the terminfo db.

ti_dir=/usr/share/terminfo

for filename in $(find ${ti_dir} -type f); do
    term_name=$(basename ${filename})
    color_count=$(tput -T ${term_name} colors | sed 's/-1/8/')
    # xm=$(tput -T ${term_name} XM)
    echo "Terminfo {\""${term_name}"\", "${color_count}"},"
    echo ${xm}
done
