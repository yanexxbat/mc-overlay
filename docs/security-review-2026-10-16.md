# Sicherheitsprüfung mc-overlay / Keim

Stand: 16.10.2026. Geprüft wurde der aktuelle Arbeitsstand einschließlich bestehender uncommitteter Änderungen. Produktivcode wurde nicht geändert.

## Umfang und Ergebnis

Gezielte Quellcodeprüfung von Lizenzprüfung, Sitzungsübergabe, Launcher, Paketentpackung, Prozessstart, Agent-Lebenszyklus, IPC und Konfigurationsdateien. Zusätzlich vorhandene Authentifizierungstests und ein isolierter Regressionstest mit einem lokalen Testsignaturschlüssel. Keine echten Lizenzschlüssel, kein Zugriff auf den produktiven Lizenzserver und kein Eingriff in eine laufende Minecraft-Instanz.

Zwei konkrete Befunde; keiner belegt eine entfernte Übernahme des Rechners. Die Prioritäten beziehen sich auf Lizenzschutz und Integrität, nicht auf eine nachgewiesene Remote-Code-Execution.

## 1. Hoch für Lizenzschutz: Agent startet ohne eigene Autorisierung

**Stellen:** `src/agent/agent_main.cpp:1764`, `:1817`, `:1822`; `tools/AgentLoader.java`; `src/common/ipc.cpp:31`, `:93`.

Beide JVM-Einstiegspunkte rufen `StartAgent` auf. Diese Funktion ignoriert die Optionen, initialisiert die JVM-Anbindung, legt den gemeinsamen Speicher an und startet den Sampler ohne Lizenzprüfung. Der mitgelieferte Java-Loader hat ebenfalls keine Lizenzprüfung. Die Prüfung in Launcher und Overlay schützt deshalb nicht den separat nutzbaren Agenten.

**Voraussetzung und Auswirkung:** Ein lokaler Benutzer mit den ausgelieferten Agent-Dateien und Zugriff auf seine eigene JVM kann den Agenten unabhängig vom lizenzierten Launcher laden. Ein eigener lokaler IPC-Client kann dessen Daten verwenden und Steueranfragen schreiben. Das originale Overlay bleibt separat geschützt; dies behauptet keinen automatischen Start der originalen Oberfläche ohne Lizenz. Die Schwere betrifft die Umgehbarkeit des Lizenzmodells, nicht eine Rechteausweitung in Windows.

**Beleg:** Durchgängiger Quellcodepfad vom Java-Loader über die exportierten Einstiegspunkte bis zum Sampler. Nicht live in Minecraft ausgeführt. Die normalen kurzlebigen Steuer-Leases prüfen Zeit und Eingabefreigabe, aber keine Lizenzidentität.

**Abhilfe:** Auch im Agenten vor Freigabe eine serverseitig signierte, kurzlebige Autorisierung prüfen, die App, Sitzung, Ablauf und eine frische Herausforderung des Agenten bindet. Autorisierung während der Laufzeit erneuern und privilegierte Agent-Funktionen bei Ablauf deaktivieren. Ein bloßes Boolean oder ein geheim gemeinter IPC-Name genügt nicht. Vollständig lokale Software bleibt grundsätzlich patchbar; diese Änderung schließt den derzeit direkt offenen Einstieg.

## 2. Mittel: Signierte Antworten sind nicht an Anfrage und Sitzung gebunden

**Stellen:** `src/common/keyauth.cpp:66`, `:130`, `:180`.

`VerifyResponse` prüft Signatur und ein Zeitfenster von ±120 Sekunden. `Decode` prüft Erfolgsstatus, Owner-ID und Code 68. `Client::Check` akzeptiert dieses allgemeine Ergebnis, ohne zu prüfen, ob die Antwort zu seiner Sitzung und zur Operation `check` gehört. Eine bereits signierte Initialisierungsantwort kann deshalb im lokalen Test eine andere, frei gewählte Sitzung bestätigen.

**Nachweis:** `build/security-audit/auth_review.cpp` nutzt dieselben Testsignaturen wie die vorhandenen Tests und die unveränderte Authentifizierungsbibliothek. Ausgabe:

```text
PASS KeyAuth: signed responses, application identity, license/session expiry, encoding, rejection, cancellation, inherited handoff
AUDIT: init response accepted as check for unrelated session: 1
```

Der Build des vorhandenen Testziels wurde geprüft; Ninja meldete es aktuell. Anschließend wurde die zusätzliche Testdatei kompiliert und ausgeführt. Das Ergebnis zeigt einen Protokoll-/Validierungsfehler, keinen praktisch vorgeführten Angriff auf TLS. Der Testschlüssel ersetzt ausschließlich den produktiven Signaturschlüssel im isolierten Test.

**Voraussetzung und Auswirkung:** Ein Angreifer muss eine noch zeitgültige, korrekt signierte Antwort derselben Owner-ID besitzen und sie in den Antwortpfad einschleusen können, etwa bei Kontrolle eines vertrauenswürdigen TLS-Proxys oder des lokalen Transports. Ein gewöhnlicher Netzwerkbeobachter kann das bei intaktem HTTPS nicht. Unter dieser Voraussetzung kann `Check` einen unzutreffenden Erfolg melden. Eine dauerhafte vollständige Lizenzumgehung durch diesen Befund allein wurde nicht nachgewiesen.

**Abhilfe:** Anfrageart, Sitzungsbindung und einen frischen Zufallswert serverseitig signieren lassen und exakt prüfen. Falls der Anbieter diese Bindung nicht unterstützt, ist dafür ein eigener Autorisierungsdienst oder eine abgestimmte API-Erweiterung nötig. Nur das Zeitfenster zu verkürzen löst die fehlende Bindung nicht. Als Regressionstest muss die oben beschriebene vertauschte Antwort künftig abgelehnt werden.

## Weitere Härtungspunkte, keine separat nachgewiesenen Exploits

- `Client::Check` fragt den Bezahlstatus nicht explizit neu ab und wertet kein `isPaid` aus. Die Ablaufzeit stammt aus dem ursprünglichen Login bzw. der lokalen Übergabe. Das offizielle SDK unterscheidet `check(false)` und `check(true)` mit `?check_paid=1`. Ob Entzug oder Verkürzung eines Abos in der konkreten Serverkonfiguration schon einen normalen Session-Check scheitern lässt, wurde nicht live geprüft. Dies sollte mit einer dedizierten Testlizenz geprüft werden.
- Die IPC-Kanäle vertrauen Prozessen desselben Windows-Benutzers. Vorhersehbare Namen und Schreibzugriff erlauben lokale Manipulationen. Das ist keine nachgewiesene kontoübergreifende Lücke. Snapshot-Daten sollten trotzdem an der Grenze vollständig validiert werden: begrenzte Zähler, endliche Zahlen und terminierte Zeichenfelder. `TryReadSnapshot` selbst prüft nur die Sequenzkonsistenz; beispielsweise verwendet die Serveranzeige später `%s`.
- Der Paketcache prüft SHA-256 vor der späteren Ausführung, hält die geprüften Dateien aber nicht durchgehend gegen Austausch gesperrt. Temporäre Paketdateien werden mit vorhersagbarem Namen und Trunkierung geöffnet. Relevant bei lokalem Schreibzugriff auf den Cache; keine Rechteausweitung nachgewiesen. Exklusiv angelegte temporäre Dateien, Reparse-Point-Prüfungen und ein klar definiertes Vertrauensmodell für den Cache verbessern die Absicherung.
- Die als HWID bezeichnete Kennung ist die Windows-Benutzer-SID (`HardwareId`), keine Hardwaremessung. Das ist insbesondere eine Einschränkung des Gerätebindungsmodells und nicht automatisch eine Sicherheitslücke.

## Positiv aufgefallen

HTTPS mit normaler Zertifikatsprüfung, deaktivierte HTTP-Weiterleitungen, Ed25519-Prüfung, begrenzte Antwortgrößen, Form-Encoding, regelmäßige Sitzungsprüfung mit Abbruch bei Fehler, eingeschränkte Handle-Vererbung bei der Sitzungsübergabe, Paket-Hashes sowie Namens-, Größen- und Schema-Prüfungen bei benannten Konfigurationen sind vorhanden.

## Grenzen

Keine vollständige Prüfung aller Render-/JNI-Pfade, kein Fuzzing, keine umfassende Abhängigkeits-/CVE-Prüfung und keine Prüfung der serverseitigen KeyAuth-Konfiguration. Die Ergebnisse sind daher keine Sicherheitsfreigabe des gesamten Clients. Bestehende grüne Funktionstests schließen die gezeigten Sicherheitsfehler nicht aus.

Referenzen zum Anbieter:
- https://docs.keyauth.cc/api/features/check-session
- https://github.com/KeyAuth/keyauth-cpp-library-1.3API/blob/main/auth.cpp (`api::check`, insbesondere `check_paid`)
