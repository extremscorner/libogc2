FROM devkitpro/devkitppc:20260117

LABEL maintainer="Extrems <extrems@extremscorner.org>" \
      org.opencontainers.image.source="https://github.com/extremscorner/libogc2"

SHELL ["/bin/bash", "-c"]
RUN rm /etc/apt/sources.list.d/devkitpro.list && \
    wget -O - https://packages.extremscorner.org/devkitpro.gpg | dkp-pacman-key --add - && \
    dkp-pacman-key --lsign-key C8A2759C315CFBC3429CC2E422B803BA8AA3D7CE && \
    sed -i '/^\[dkp-libs\]$/,$d' /opt/devkitpro/pacman/etc/pacman.conf && \
    echo -e '[extremscorner-devkitpro]\nServer = https://packages.extremscorner.org/devkitpro/linux/$arch' >> /opt/devkitpro/pacman/etc/pacman.conf && \
    dkp-pacman -Syy && \
    dkp-pacman -S --ask 5 --ignore *-docs*,*-examples* gamecube-dev gamecube-portlibs ppc-portlibs wii-dev wii-portlibs && \
    yes | dkp-pacman -Scc
