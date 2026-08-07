# Raspberry Pi Deploy

Einmalig den Public Key auf den Pi kopieren:

```powershell
cd C:\Users\alexn\Downloads\light
Get-Content .\raspi_dmx_deploy_key.pub | ssh root@raspi-dmx "mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys && chmod 700 ~/.ssh && chmod 600 ~/.ssh/authorized_keys"
```

Danach deployen:

```powershell
.\deploy_to_raspi.ps1
```

Die C++-Weboberflaeche laeuft danach im gesamten Heimnetz auf:

```text
http://raspi-dmx.local/
```

In Netzen mit lokaler Kurznamensauflösung funktioniert zusätzlich `http://raspi-dmx/`.

VirtualDJ:

```text
os2l = yes
os2lDirectIp = raspi-dmx:9996
```

Der bestehende ArtNet-DMX-Bridge-Prozess wird als `artnet-dmx-current` gestartet, falls `/root/raspi-dmx-node` vorhanden ist.

Aktueller Pi-Stand:

```text
light-engine-cpp:    OpenRC default, Port 80 und 9996, ArtNet lokal an 127.0.0.1
artnet-dmx-current: OpenRC default, /root/raspi-dmx-node auf /dev/ttyAMA0
Web:                http://raspi-dmx/web
VirtualDJ:          os2lDirectIp = raspi-dmx:9996
```

Der serielle Login auf `ttyAMA0` wurde deaktiviert, damit der UART sauber fuer DMX benutzt werden kann. Backup: `/etc/inittab.light-engine.bak`.
