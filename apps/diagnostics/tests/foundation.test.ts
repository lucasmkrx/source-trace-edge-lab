import assert from "node:assert/strict";
import test from "node:test";

import { diagnosticProtocolVersion } from "../src/foundation.js";

test("diagnostic control protocol starts at version one", () => {
  assert.equal(diagnosticProtocolVersion, 1);
});
