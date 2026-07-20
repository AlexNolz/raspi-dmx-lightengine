# QLC+ Fixtures

Importiere `Eurolite-LED-TMH-17.qxf` in QLC+, falls QLC+ das Fixture nicht schon mitliefert.

Importiere `Stairville-LED-Bar-240-8-RGB.qxf`, damit die LED Bars nicht mehr mit `Luminous flux 0lm` in der 3D View stehen. Diese Definition hat die gleichen Masse wie deine Bar aus QLC+:

- 1064 x 88 x 65 mm
- 36 W
- 24-channel RGB
- 1200 lm Visualizer-Lichtstrom
- 60 Grad Linse

`Visualizer-RGB-Wide-Wash.qxf` war ein aelterer Versuch mit groesseren Fake-Flutern. Der Versuch ist nicht mehr im 3D-Testprojekt eingebaut.

In QLC+:

1. Fixtures oeffnen
2. Fixture Definition Editor / Import nutzen
3. `Eurolite-LED-TMH-17.qxf` importieren
4. `Stairville-LED-Bar-240-8-RGB.qxf` importieren
5. `3d_test.qxw` neu oeffnen

Quelle der Fixturedefinition: offizielles QLC+ Fixture-Repository, `resources/fixtures/Eurolite/Eurolite-LED-TMH-17.qxf`.
