cp config-um .config

make -j16 CONFIG_DEBUG_SECTION_MISMATCH=y ARCH=um SUBARCH=x86_64
