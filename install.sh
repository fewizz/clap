[ -z "$1" ] && echo "arg is root for include dir" \
|| ( \
    echo "uninstalling..." \
    && \
    rm -rfv $1/include/clap \
    && \
    echo "installing..." \
    && \
    cp -rv include $1 \
    &&
    cp -v compile_flags.txt $1/include/clap \
)