"""Помощники для цифрового генома Kolibri и формата KolibriScript Digits (.ksd)."""

from __future__ import annotations

import dataclasses
import hashlib
import hmac
import json
import os
from pathlib import Path
from typing import Any, Iterable, List, Mapping, Sequence

__all__ = [
    "KsdValidationError",
    "KolibriGenomeLedger",
    "SecretsConfig",
    "deserialize_ksd",
    "load_secrets_config",
    "serialize_ksd",
]


KSD_MAGIC = "707"
_LEN_DIGITS = 6
_SIGNATURE_DIGITS = 32


class KsdValidationError(ValueError):
    """Исключение, сигнализирующее о повреждении .ksd."""


@dataclasses.dataclass(frozen=True)
class SecretsConfig:
    """Конфигурация секретов KolibriScript."""

    hmac_key: bytes

    @classmethod
    def from_mapping(cls, payload: Mapping[str, Any]) -> "SecretsConfig":
        """Создаёт конфигурацию из словаря.

        Ожидается, что ключ HMAC хранится по одному из путей:
        - ``payload["hmac_key"]``
        - ``payload["kolibri"]["script"]["hmac_key"]``
        - ``payload["kolibri_hmac_key"]``

        Значение может быть строкой UTF-8, ``hex:`` или ``base64:``.
        """

        candidates: Iterable[Mapping[str, Any]]
        if "kolibri" in payload and isinstance(payload["kolibri"], Mapping):
            kolibri_section = payload["kolibri"]
        else:
            kolibri_section = {}
        if isinstance(kolibri_section, Mapping):
            script_section = kolibri_section.get("script", {})
        else:
            script_section = {}
        candidates = (
            payload,
            kolibri_section if isinstance(kolibri_section, Mapping) else {},
            script_section if isinstance(script_section, Mapping) else {},
        )
        key_value: Any = None
        for mapping in candidates:
            if not isinstance(mapping, Mapping):
                continue
            if "hmac_key" in mapping:
                key_value = mapping["hmac_key"]
                break
            if "kolibri_hmac_key" in mapping:
                key_value = mapping["kolibri_hmac_key"]
                break
        if isinstance(key_value, bytes):
            return cls(hmac_key=bytes(key_value))
        if isinstance(key_value, str):
            return cls(hmac_key=_decode_secret_string(key_value))
        raise ValueError("в конфигурации секретов отсутствует hmac_key")


def load_secrets_config(path: "str | os.PathLike[str] | None" = None) -> SecretsConfig:
    """Загружает конфигурацию секретов из JSON-файла."""

    candidates: List[Path] = []
    if path is not None:
        candidates.append(Path(path))
    env_path = os.getenv("KOLIBRI_SECRETS_PATH")
    if env_path:
        candidates.append(Path(env_path))
    candidates.append(Path.cwd() / "kolibri_secrets.json")
    candidates.append(Path.home() / ".config/kolibri/secrets.json")

    for candidate in candidates:
        if candidate.is_file():
            data = json.loads(candidate.read_text(encoding="utf-8"))
            if not isinstance(data, Mapping):
                raise ValueError("секреты должны быть JSON-объектом")
            return SecretsConfig.from_mapping(data)
    raise FileNotFoundError("не удалось найти файл конфигурации секретов")


@dataclasses.dataclass(frozen=True)
class KsdDocument:
    """Декодированное содержимое .ksd-файла."""

    tokens: Sequence[str]
    records: Sequence[Mapping[str, Any]]


def serialize_ksd(records: Sequence[Mapping[str, Any]], secrets: SecretsConfig) -> str:
    """Сериализует записи цифрового генома в поток цифр KolibriScript."""

    normalized_records = _normalize_records(records)
    tokens = sorted(_collect_tokens(normalized_records))
    table_digits = _text_to_digits(
        json.dumps(tokens, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    )
    payload_digits = _text_to_digits(
        json.dumps(normalized_records, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    )
    body = _build_body(table_digits, payload_digits)
    signature = _compute_signature(body, secrets.hmac_key)
    return body + signature


def deserialize_ksd(data: str, secrets: SecretsConfig) -> KsdDocument:
    """Десериализует поток цифр KolibriScript и валидирует подпись HMAC."""

    digits = data.strip()
    if not digits:
        return KsdDocument(tokens=[], records=[])
    if not digits.isdigit():
        raise KsdValidationError("поток содержит недопустимые символы")

    if len(digits) < len(KSD_MAGIC) + 2 * _LEN_DIGITS + _SIGNATURE_DIGITS:
        raise KsdValidationError("формат .ksd неполон")

    body = digits[:-_SIGNATURE_DIGITS]
    signature = digits[-_SIGNATURE_DIGITS:]
    expected = _compute_signature(body, secrets.hmac_key)
    if signature != expected:
        raise KsdValidationError("контрольная подпись HMAC не совпадает")

    index = 0
    if body[: len(KSD_MAGIC)] != KSD_MAGIC:
        raise KsdValidationError("отсутствует сигнатура формата .ksd")
    index += len(KSD_MAGIC)

    table_len = _parse_length(body[index : index + _LEN_DIGITS])
    index += _LEN_DIGITS
    table_digits = body[index : index + table_len]
    index += table_len

    payload_len = _parse_length(body[index : index + _LEN_DIGITS])
    index += _LEN_DIGITS
    payload_digits = body[index : index + payload_len]
    index += payload_len

    if index != len(body):
        raise KsdValidationError("формат .ksd имеет лишние данные")

    tokens_text = _digits_to_text(table_digits)
    payload_text = _digits_to_text(payload_digits)
    tokens = json.loads(tokens_text) if tokens_text else []
    records = json.loads(payload_text) if payload_text else []

    if not isinstance(tokens, list) or not all(isinstance(t, str) for t in tokens):
        raise KsdValidationError("таблица токенов повреждена")
    if not isinstance(records, list):
        raise KsdValidationError("полезная нагрузка .ksd повреждена")

    coerced_records: List[Mapping[str, Any]] = []
    for record in records:
        if isinstance(record, Mapping):
            coerced_records.append(dict(record))
        else:
            raise KsdValidationError("ожидается список словарей событий")

    return KsdDocument(tokens=tokens, records=coerced_records)


class KolibriGenomeLedger:
    """Файловый журнал KolibriScript с атомарным обновлением."""

    def __init__(self, path: Path, secrets: SecretsConfig) -> None:
        self.path = Path(path)
        self._secrets = secrets
        self._records: List[Mapping[str, Any]] = []
        self.path.parent.mkdir(parents=True, exist_ok=True)
        if self.path.exists():
            raw = self.path.read_text(encoding="utf-8")
            if raw.strip():
                document = deserialize_ksd(raw, secrets)
                self._records = [dict(record) for record in document.records]

    @property
    def records(self) -> Sequence[Mapping[str, Any]]:
        """Возвращает копию текущих записей."""

        return [dict(record) for record in self._records]

    def append(self, block: Mapping[str, Any], journal_entry: Mapping[str, Any]) -> None:
        """Добавляет запись в журнал и сбрасывает файл atomically."""

        block_dict = dict(_ensure_plain_mapping(block))
        entry_dict = dict(_ensure_plain_mapping(journal_entry))
        entry_dict["block"] = block_dict
        self._records.append(entry_dict)
        self._flush()

    def _flush(self) -> None:
        serialized = serialize_ksd(self._records, self._secrets)
        tmp_path = self.path.with_name(f".{self.path.name}.tmp")
        with tmp_path.open("w", encoding="utf-8") as handle:
            handle.write(serialized)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(tmp_path, self.path)


def _ensure_plain_mapping(value: Any) -> Mapping[str, Any]:
    plain = _to_plain(value)
    if not isinstance(plain, Mapping):
        raise TypeError("ожидалось отображение")
    return dict(plain)


def _collect_tokens(records: Sequence[Mapping[str, Any]]) -> Iterable[str]:
    for record in records:
        yield from _collect_from_value(record)


def _collect_from_value(value: Any) -> Iterable[str]:
    if isinstance(value, Mapping):
        for key, val in value.items():
            yield str(key)
            yield from _collect_from_value(val)
    elif isinstance(value, (list, tuple)):
        for item in value:
            yield from _collect_from_value(item)
    elif isinstance(value, (str, int, float, bool)) or value is None:
        yield str(value)


def _to_plain(value: Any) -> Any:
    if dataclasses.is_dataclass(value) and not isinstance(value, type):
        return _to_plain(dataclasses.asdict(value))
    if isinstance(value, Mapping):
        return {str(key): _to_plain(val) for key, val in value.items()}
    if isinstance(value, (list, tuple)):
        return [_to_plain(item) for item in value]
    return value


def _normalize_records(records: Sequence[Mapping[str, Any]]) -> List[Mapping[str, Any]]:
    normalized: List[Mapping[str, Any]] = []
    for record in records:
        if not isinstance(record, Mapping):
            raise TypeError("каждая запись генома должна быть отображением")
        plain = _to_plain(record)
        if not isinstance(plain, Mapping):
            raise TypeError("каждая запись генома должна быть отображением")
        normalized.append(dict(plain))
    return normalized


def _build_body(table_digits: str, payload_digits: str) -> str:
    table_len = _format_length(len(table_digits))
    payload_len = _format_length(len(payload_digits))
    return f"{KSD_MAGIC}{table_len}{table_digits}{payload_len}{payload_digits}"


def _format_length(length: int) -> str:
    if length < 0 or length >= 10**_LEN_DIGITS:
        raise ValueError("недопустимая длина поля .ksd")
    return f"{length:0{_LEN_DIGITS}d}"


def _parse_length(digits: str) -> int:
    if len(digits) != _LEN_DIGITS or not digits.isdigit():
        raise KsdValidationError("длина поля .ksd повреждена")
    return int(digits)


def _text_to_digits(text: str) -> str:
    data = text.encode("utf-8")
    return "".join(f"{byte:03d}" for byte in data)


def _digits_to_text(digits: str) -> str:
    if len(digits) % 3 != 0:
        raise KsdValidationError("цифровой поток имеет неверную длину")
    chunks = (digits[index : index + 3] for index in range(0, len(digits), 3))
    try:
        data = bytes(int(chunk) for chunk in chunks)
    except ValueError as exc:
        raise KsdValidationError("цифровой поток повреждён") from exc
    return data.decode("utf-8")


def _compute_signature(body: str, key: bytes) -> str:
    digest = hmac.new(key, body.encode("ascii"), hashlib.sha256).digest()
    return "".join(str(byte % 10) for byte in digest)


def _decode_secret_string(value: str) -> bytes:
    trimmed = value.strip()
    if trimmed.startswith("hex:"):
        hex_value = trimmed[4:]
        return bytes.fromhex(hex_value)
    if trimmed.startswith("base64:"):
        import base64

        return base64.b64decode(trimmed[7:])
    return trimmed.encode("utf-8")
