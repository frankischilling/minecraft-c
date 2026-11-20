# Original precipitation prefixes

`PrecipitationObservation.java` invokes unchanged `px.c(F)V` from the verified
Beta 1.7.3 client archive. PX, App and World receivers are prepared without their
constructors. App's camera stays null. Ten raw strength operands crossed with
ten raw partial ticks yield 100 return/null outcomes and subsequent Random values.
The observer refuses network, file writes, listeners and native links.

`finite.json` stores those public observations. `source-pins.json` records complete
class hashes and method Code identities. Native `client.precipitation` compares
every row. Its additional rendering and cleanup controls have separate native
scope; no original rendered-weather equality follows from these prefix rows.

To recapture with a matching JDK17 and locally supplied libraries:

```sh
python -B tests/reference/client/precipitation/capture_reference.py \
  --client-jar /path/to/b1.7.3.jar \
  --library-root /path/to/.minecraft/libraries \
  --output .local/precipitation-original-new
```

The output directory must not exist. The capture validates pinned archive and
SDK hashes, checks class/method identities and records tool/helper/input hashes.
Original files and compiled helpers must remain unchanged through observation.
Java is needed only for this optional reference capture, not native gameplay or
the public CTest replay. See [runtime behavior and limits](../../../../docs/client/precipitation.md).
