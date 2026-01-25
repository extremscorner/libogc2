# Maintainer: Extrems <extrems@extremscorner.org>

_pkgname=libogc2
pkgname=(${_pkgname}{,-docs}-git)
pkgver=r2354.789670c
pkgrel=2
pkgdesc='C library for GameCube and Wii targeting devkitPPC.'
arch=('any')
url='https://github.com/extremscorner/libogc2'
license=('custom')
groups=('gamecube-dev' 'wii-dev')
depends=('devkitPPC>=r42' 'devkitPPC<r49' 'gamecube-tools' 'ppc-libmad')
makedepends=('doxygen' 'git')
options=(!strip libtool staticlibs)

prepare() {
	cd "${startdir}"
	git update-index --assume-unchanged PKGBUILD
}

pkgver() {
	cd "${startdir}"
	printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short=7 HEAD)"
}

build() {
	cd "${startdir}"
	make
	make docs
}

package_libogc2-git() {
	optdepends=(${_pkgname}{-docs-git,-examples})
	provides=("${_pkgname}")
	conflicts=("${_pkgname}")

	cd "${startdir}"
	DESTDIR="${pkgdir}" make install
}

package_libogc2-docs-git() {
	pkgdesc+=' (documentation)'
	depends=("${_pkgname}-git")
	provides=("${_pkgname}-docs")
	conflicts=("${_pkgname}-docs")

	cd "${startdir}"
	for platform in gamecube wii; do
		mkdir -p "${pkgdir}${DEVKITPRO}/libogc2/${platform}/share/doc"
		cp -r docs "${pkgdir}${DEVKITPRO}/libogc2/${platform}/share/doc/${_pkgname}"
	done
}
