Name: xdriinfo
Version: 1.0.2
Release: alt1
Summary: query configuration information of DRI drivers
License: MIT/X11
Group: System/X11
Url: http://xorg.freedesktop.org
Packager: Valery Inozemtsev <shrek@altlinux.ru>

Source: %name-%version.tar.bz2

BuildRequires: xorg-glproto-devel libX11-devel libGL-devel
BuildRequires: xorg-util-macros libXdmcp-devel libXau-devel

%description
xdriinfo  can be used to query configuration information of direct ren-
dering drivers. If no command argument is specified it lists the  names
of the direct rendering drivers for all screens

%prep
%setup -q

%build
%autoreconf
%configure

%make

%install
%make DESTDIR=%buildroot install

%files
%_bindir/*
%_man1dir/*

%changelog
* Sat Aug 18 2007 Valery Inozemtsev <shrek@altlinux.ru> 1.0.2-alt1
- 1.0.2

* Tue Dec 27 2005 Valery Inozemtsev <shrek@altlinux.ru> 1.0.0-alt1
- Xorg-7.0

* Sun Dec 04 2005 Valery Inozemtsev <shrek@altlinux.ru> 0.99.2-alt1
- Xorg-7.0RC3

* Thu Nov 24 2005 Valery Inozemtsev <shrek@altlinux.ru> 0.99.1-alt0.1
- initial release

