In diesem Repo findet Ihr weiterführende Dateien und Informationen zu unserer Modifikation des Ringspiels.

<img src="images/DSCF7820_Aufmacherfoto.JPG" width="400">


# Aufbau:

## Gesamtabmessungen

<img src="images/CAD.png" width="400"> 

## Fräsungen

Verkabelung im oberen Querbalken:

<img src="images/DSCF7830_Querbalken_innen.JPG" width="400"> 

Verkabelung auf der Unterseite: 

<img src="images/DSCF7837_Unterseite.JPG" width="400">

<img src="images/DSCF7838_Anschluss_Querbalken.JPG" width="400">

## Haken und Kette

<img src="images/DSCF7806_Ring+Haken.JPG" width="400">

<img src="images/DSCF7831_Kette_ueberbruecken.JPG" width="400">


# Informationen zur Konfiguration des USB-C Ports

Die VBUS-Versorgungsleitung eines USB-C Kabels ist standardmäßig spannungsfrei. Das ist grundsätzlich anders als bei USB-A oder USB-B Verbindungen. 
Damit USB-C Netzteile eine Spannung ausgeben, muss diesem über einen sogenannten CC (Configuration Channel) mittgeteilt werden, das ein Gerät angeschlossen ist. Um eine Spannung von 5V zu erhalten, reichen zwei 5.1kOhm Widerstände zwischen CC1/CC2 und GND.
Diese sind praktischerweise auf USB-C Breakout Boards direkt verbaut. 
