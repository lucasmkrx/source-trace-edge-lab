from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


def build_pdf() -> bytes:
    content = (
        b"BT\n"
        b"/F1 18 Tf\n"
        b"72 720 Td\n"
        b"(SourceTrace Edge Lab - Synthetic Demo) Tj\n"
        b"0 -32 Td\n"
        b"/F1 12 Tf\n"
        b"(Measure twice. Record deterministic evidence.) Tj\n"
        b"0 -22 Td\n"
        b"(Fixture ID: ORIGINAL-PDF-V1) Tj\n"
        b"ET\n"
    )
    objects = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        (
            b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
            b"/Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>"
        ),
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
        b"<< /Length "
        + str(len(content)).encode("ascii")
        + b" >>\nstream\n"
        + content
        + b"endstream",
    ]

    # NUL is valid in the binary marker and ensures source-control tools treat
    # the fixture as binary instead of rewriting fixed-width xref records.
    document = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\x00\n")
    offsets = [0]
    for number, body in enumerate(objects, start=1):
        offsets.append(len(document))
        document.extend(f"{number} 0 obj\n".encode())
        document.extend(body)
        document.extend(b"\nendobj\n")

    xref_offset = len(document)
    document.extend(f"xref\n0 {len(objects) + 1}\n".encode())
    document.extend(b"0000000000 65535 f \n")
    for offset in offsets[1:]:
        document.extend(f"{offset:010d} 00000 n \n".encode())
    document.extend(
        (
            f"trailer\n<< /Size {len(objects) + 1} /Root 1 0 R "
            f"/ID [<53544c41424649585455524530303031>"
            f"<53544c41424649585455524530303031>] >>\n"
            f"startxref\n{xref_offset}\n%%EOF\n"
        ).encode()
    )
    return bytes(document)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).parents[1] / "tests/fixtures/original-demo.pdf",
    )
    arguments = parser.parse_args()
    data = build_pdf()
    arguments.output.write_bytes(data)
    print(f"{arguments.output} sha256={hashlib.sha256(data).hexdigest()}")


if __name__ == "__main__":
    main()
