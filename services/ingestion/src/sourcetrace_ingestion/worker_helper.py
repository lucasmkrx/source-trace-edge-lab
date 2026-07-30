from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


class InvalidPdfError(Exception):
    pass


class PageLimitError(Exception):
    pass


class ConversionError(Exception):
    pass


def apply_resource_limits(memory_bytes: int, output_bytes: int, cpu_seconds: int) -> None:
    try:
        import resource
    except ImportError:
        return

    limits = (
        (resource.RLIMIT_AS, memory_bytes),
        (resource.RLIMIT_FSIZE, output_bytes),
        (resource.RLIMIT_CPU, cpu_seconds),
        (resource.RLIMIT_NOFILE, 32),
    )
    for resource_name, value in limits:
        try:
            resource.setrlimit(resource_name, (value, value))
        except (OSError, ValueError):
            continue


def process(
    input_path: Path,
    output_path: Path,
    max_pages: int,
    pdftotext_binary: str,
    conversion_timeout_seconds: float,
) -> dict[str, int]:
    from pypdf import PdfReader
    from pypdf.errors import PdfReadError

    try:
        reader = PdfReader(input_path, strict=True)
        if reader.is_encrypted:
            raise PdfReadError("encrypted PDFs are unsupported")
        page_count = len(reader.pages)
        if page_count == 0:
            raise PdfReadError("PDF has no pages")
        if page_count > max_pages:
            raise PageLimitError
    except PageLimitError:
        raise
    except (PdfReadError, OSError, ValueError) as error:
        raise InvalidPdfError from error

    try:
        subprocess.run(  # noqa: S603 - configured binary, fixed options, and shell disabled
            [
                pdftotext_binary,
                "-enc",
                "UTF-8",
                "-layout",
                str(input_path),
                str(output_path),
            ],
            shell=False,
            check=True,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            timeout=conversion_timeout_seconds,
        )
    except (OSError, subprocess.SubprocessError) as error:
        raise ConversionError from error
    return {"pageCount": page_count, "textBytes": output_path.stat().st_size}


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--max-pages", type=int, required=True)
    parser.add_argument("--memory-bytes", type=int, required=True)
    parser.add_argument("--output-bytes", type=int, required=True)
    parser.add_argument("--cpu-seconds", type=int, required=True)
    parser.add_argument("--conversion-timeout-seconds", type=float, required=True)
    parser.add_argument("--pdftotext", required=True)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    apply_resource_limits(
        arguments.memory_bytes,
        arguments.output_bytes,
        arguments.cpu_seconds,
    )
    try:
        result = process(
            arguments.input,
            arguments.output,
            arguments.max_pages,
            arguments.pdftotext,
            arguments.conversion_timeout_seconds,
        )
    except InvalidPdfError:
        return 10
    except PageLimitError:
        return 11
    except ConversionError:
        return 12
    print(json.dumps(result, separators=(",", ":"), sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
