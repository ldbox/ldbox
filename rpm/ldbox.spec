Summary: 	ldbox crosscompiling environment
License: 	LGPL
URL:		https://github.com/mer-packages/ldbox
Name: 		ldbox
Version:	2.3.90
Release:	14
Source: 	%{name}-%{version}.tar.gz
Prefix: 	/usr
Group: 		Development/Tools
ExclusiveArch:	%{ix86} x86_64
BuildRequires:	make
BuildRequires:	autoconf
Requires:	fakeroot
Requires:	liblb = %{version}-%{release}

%description
ldbox crosscompiling environment

%package -n liblb
Summary: ldbox preload library
Group:   Development/Tools

%description -n liblb
ldbox preload library.

%prep
%setup -q -n %{name}-%{version}

%build
./autogen.sh
%configure
touch .configure
make

%install
make install DESTDIR=%{buildroot}

install -D -m 644 utils/lb.bash %{buildroot}/etc/bash_completion.d/lb.bash

%files
%defattr(-,root,root)
%{_bindir}/lb*
%{_datadir}/ldbox
%config %{_sysconfdir}/bash_completion.d/lb.bash
%doc %attr(0444,root,root) %{_mandir}/man1/*
%doc %attr(0444,root,root) %{_mandir}/man7/*

%files -n liblb
%defattr(-,root,root)
%{_libdir}/liblb
