# Mini Light Engine

Diese Engine ersetzt QLC+ fuer den Live-Test: Sie nimmt VirtualDJ OS2L direkt an und sendet ArtNet direkt zur LED-Bar.

Start:

```powershell
cd C:\Users\alexn\Downloads\light
python .\light_engine.py
```

Dann im Browser oeffnen:

```text
http://127.0.0.1:8088
```

Die Engine startet absichtlich gestoppt. Erst in der Weboberflaeche auf `Start` druecken, wenn die LED-Bar reagieren soll.

VirtualDJ:

```text
os2l = yes
os2lDirectIp = 127.0.0.1:9996
```

## VirtualDJ OS2L Buttons

Die fuenf Lichtbefehle werden in VirtualDJ als eigene Pad- oder Custom-Buttons
angelegt. VirtualDJ entdeckt diese Buttons nicht automatisch. Als jeweilige
VDJScript-Action verwenden:

```text
White Out:     os2l_button "whiteout" while_pressed
Color Strobe:  os2l_button "color_strobe" while_pressed
Strobe Out:    os2l_button "strobe_out" while_pressed
Fog Burst:     os2l_button "fog"
Blackout:      os2l_button "blackout" while_pressed
```

Alle vier Hold-Effekte (`White Out`, `Color Strobe`, `Strobe Out`, `Blackout`)
enden dadurch sofort beim Loslassen. Die Strobe-Geschwindigkeit aus der
Weboberflaeche gilt fuer beide Strobe-Effekte.

Beim vorhandenen VirtualDJ-Pad `DMX` mit dem Profil `VDJ 2018` sind die
Beschriftungen fest. Die Engine ordnet sie so zu:

```text
blackout = Blackout
strobe   = Color Strobe
fog      = Fog Burst
next     = Next Look
cmd 1    = White Out
cmd 2    = Strobe Out
```

Defaults:

```text
ArtNet Ziel: 192.168.137.255
Universe: 0
DMX Start: 3
Segmente: 8 RGB
```

QLC+ sollte geschlossen sein, weil sonst Port 9996 und die DMX-Ausgabe konkurrieren koennen.

Farbstrategie:

```text
Die generativen Szenen nutzen klare RGB-nahe Farben und begrenzen normale Looks auf maximal zwei aktive RGB-Kanaele.
Nur der White-Out Button sendet echtes Weiss.
```

Presets:

```text
Lounge: weichere Pulse, Ball, Pair Swap, Comet, Fill
Club: breite Mischung aus Chase, Scanner, Blocks, Sparkle, Orbit, Binary
Rave: harte Gates, Strobe, Siren, Binary, schnelle Bewegungen
Game Show: Ball, Zipper, Theater, Traffic, Siren
Pure RGB: fast nur Rot/Gruen/Blau mit klaren Segmentmustern
```

## Raspberry Pi / Alpine

Auf einem Raspberry Pi muss die Engine im Netzwerk lauschen, nicht nur auf `127.0.0.1`.

Alpine:

```sh
apk add python3
cd /opt/light
python3 light_engine.py --web-host 0.0.0.0 --web-port 80 --os2l-host 0.0.0.0 --artnet-host 127.0.0.1 --fps 30
```

VirtualDJ auf dem Windows-Rechner:

```text
os2l = yes
os2lDirectIp = <RASPI-IP>:9996
```

Weboberflaeche:

```text
http://<RASPI-IP>/web
```

Wenn der Pi selbst nur ArtNet zu DMX umsetzt, kann `--artnet-host 127.0.0.1` passen. Wenn ein anderer ArtNet-Node DMX sendet, dort die Node-IP oder die Broadcast-Adresse eintragen.
