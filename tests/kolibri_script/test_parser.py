import sys  # noqa: E402
import textwrap
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if ROOT.name == "tests":
    ROOT = ROOT.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from core.kolibri_script.parser import (
    CreateFormula,
    Diagnostic,
    DropFormula,
    EvaluateFormula,
    IfStatement,
    ParseResult,
    Program,
    SaveFormula,
    ShowStatement,
    TeachAssociation,
    VariableDeclaration,
    parse_script,
)


def test_parse_valid_program() -> None:
    script = textwrap.dedent(
        """
        начало:
            показать "Kolibri готов к обучению"
            переменная память = "фрактал"
            обучить связь "привет" -> "здравствуй"
            создать формулу сумма из "a + b"
            оценить сумма на задаче "2+2"
            если фитнес сумма > 0.8 тогда
                сохранить сумма в геном
            иначе
                отбросить сумма
            конец
        конец.
        """
    ).strip()

    result = parse_script(script)

    assert isinstance(result, ParseResult)
    assert result.diagnostics == []
    assert isinstance(result.program, Program)

    statements = list(result.program.statements)
    assert len(statements) == 6

    show = statements[0]
    assert isinstance(show, ShowStatement)
    assert show.value.text == '"Kolibri готов к обучению"'

    variable = statements[1]
    assert isinstance(variable, VariableDeclaration)
    assert variable.name == "память"
    assert variable.value.text == '"фрактал"'

    teach = statements[2]
    assert isinstance(teach, TeachAssociation)
    assert teach.left.text == '"привет"'
    assert teach.right.text == '"здравствуй"'

    create = statements[3]
    assert isinstance(create, CreateFormula)
    assert create.name == "сумма"
    assert create.expression.text == '"a + b"'

    evaluate = statements[4]
    assert isinstance(evaluate, EvaluateFormula)
    assert evaluate.name == "сумма"
    assert evaluate.task.text == '"2+2"'

    last = statements[5]
    assert isinstance(last, IfStatement)
    assert last.condition.text == "фитнес сумма > 0.8"
    assert len(last.then_body) == 1
    assert isinstance(last.then_body[0], SaveFormula)
    assert len(last.else_body) == 1
    assert isinstance(last.else_body[0], DropFormula)


def test_reports_mismatched_if_block() -> None:
    script = textwrap.dedent(
        """
        начало:
            если истина тогда
                показать "ok"
        конец.
        """
    ).strip()

    result = parse_script(script)

    assert result.program is not None
    assert any(diag.code == 2 for diag in result.diagnostics)


def test_reports_unknown_command() -> None:
    script = textwrap.dedent(
        """
        начало:
            телепортировать "Kolibri"
        конец.
        """
    ).strip()

    result = parse_script(script)

    assert result.program is not None
    assert result.diagnostics, "Expected diagnostics for unknown command"
    first = result.diagnostics[0]
    assert isinstance(first, Diagnostic)
    assert first.code == 1
