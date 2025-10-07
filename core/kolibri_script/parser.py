"""LL(1)-парсер KolibriScript с диагностикой."""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Iterable, Iterator, List, Optional, Sequence


@dataclass(frozen=True)
class SourceLocation:
    line: int
    column: int


@dataclass(frozen=True)
class SourceSpan:
    start: SourceLocation
    end: SourceLocation


@dataclass(frozen=True)
class Diagnostic:
    code: int
    message: str
    span: SourceSpan


@dataclass(frozen=True)
class Token:
    type: str
    value: str
    span: SourceSpan


@dataclass(frozen=True)
class Expression:
    text: str
    span: SourceSpan


@dataclass(frozen=True)
class Node:
    span: SourceSpan


@dataclass(frozen=True)
class ShowStatement(Node):
    value: Expression


@dataclass(frozen=True)
class VariableDeclaration(Node):
    name: str
    value: Expression


@dataclass(frozen=True)
class TeachAssociation(Node):
    left: Expression
    right: Expression


@dataclass(frozen=True)
class CreateFormula(Node):
    name: str
    expression: Expression


@dataclass(frozen=True)
class EvaluateFormula(Node):
    name: str
    task: Expression


@dataclass(frozen=True)
class SaveFormula(Node):
    name: str


@dataclass(frozen=True)
class DropFormula(Node):
    name: str


@dataclass(frozen=True)
class CallEvolution(Node):
    pass


@dataclass(frozen=True)
class PrintCanvas(Node):
    pass


@dataclass(frozen=True)
class SwarmSend(Node):
    name: str


@dataclass(frozen=True)
class IfStatement(Node):
    condition: Expression
    then_body: Sequence[Node]
    else_body: Sequence[Node]


@dataclass(frozen=True)
class WhileStatement(Node):
    condition: Expression
    body: Sequence[Node]


@dataclass(frozen=True)
class Program(Node):
    statements: Sequence[Node]


@dataclass
class ParseResult:
    program: Optional[Program]
    diagnostics: List[Diagnostic] = field(default_factory=list)


class Lexer:
    KEYWORDS = {
        "начало",
        "конец",
        "переменная",
        "показать",
        "обучить",
        "связь",
        "создать",
        "формулу",
        "из",
        "оценить",
        "на",
        "задаче",
        "если",
        "тогда",
        "иначе",
        "пока",
        "делать",
        "сохранить",
        "в",
        "геном",
        "отбросить",
        "вызвать",
        "эволюцию",
        "распечатать",
        "канву",
        "рой",
        "отправить",
    }

    SIMPLE_TOKENS = {
        ":": "COLON",
        ".": "DOT",
        "=": "ASSIGN",
        ">": "GREATER",
    }

    def __init__(self, source: str) -> None:
        self.source = source
        self.length = len(source)
        self.index = 0
        self.line = 1
        self.column = 1

    def __iter__(self) -> Iterator[Token]:
        while self.index < self.length:
            ch = self.source[self.index]
            if ch in " \t\r":
                self._advance(ch)
                continue
            if ch == "\n":
                yield self._make_token("NEWLINE", "\n", length=1)
                self.index += 1
                self.line += 1
                self.column = 1
                continue
            if ch == "-" and self._peek() == ">":
                start = self._location()
                self.index += 2
                self.column += 2
                yield Token("ARROW", "->", SourceSpan(start, self._location(back=1)))
                continue
            if ch == '"':
                yield self._read_string()
                continue
            if ch.isdigit():
                yield self._read_number()
                continue
            if ch.isalpha() or ch == "_":
                yield self._read_identifier()
                continue
            if ch in self.SIMPLE_TOKENS:
                token = self._make_token(self.SIMPLE_TOKENS[ch], ch, length=1)
                self.index += 1
                self.column += 1
                yield token
                continue
            raise ValueError(f"Unexpected character {ch!r} at {self.line}:{self.column}")
        yield Token("EOF", "", SourceSpan(self._location(), self._location()))

    def _advance(self, ch: str) -> None:
        self.index += 1
        if ch == "\n":
            self.line += 1
            self.column = 1
        else:
            self.column += 1

    def _location(self, *, back: int = 0) -> SourceLocation:
        return SourceLocation(self.line, self.column - back)

    def _make_token(self, type_: str, value: str, *, length: int) -> Token:
        start = SourceLocation(self.line, self.column)
        end = SourceLocation(self.line, self.column + length - 1)
        return Token(type_, value, SourceSpan(start, end))

    def _peek(self) -> Optional[str]:
        if self.index + 1 >= self.length:
            return None
        return self.source[self.index + 1]

    def _read_string(self) -> Token:
        start_location = SourceLocation(self.line, self.column)
        self.index += 1
        self.column += 1
        value_chars: List[str] = []
        while self.index < self.length:
            ch = self.source[self.index]
            if ch == '"':
                end_location = SourceLocation(self.line, self.column)
                self.index += 1
                self.column += 1
                span = SourceSpan(start_location, SourceLocation(end_location.line, end_location.column))
                literal = '"' + "".join(value_chars) + '"'
                return Token("STRING", literal, span)
            if ch == "\n":
                raise ValueError("Unterminated string literal")
            value_chars.append(ch)
            self.index += 1
            self.column += 1
        raise ValueError("Unterminated string literal")

    def _read_number(self) -> Token:
        start_location = SourceLocation(self.line, self.column)
        value_chars: List[str] = []
        while self.index < self.length and (self.source[self.index].isdigit() or self.source[self.index] == "."):
            value_chars.append(self.source[self.index])
            self.index += 1
            self.column += 1
        end_location = SourceLocation(self.line, self.column - 1)
        return Token("NUMBER", "".join(value_chars), SourceSpan(start_location, end_location))

    def _read_identifier(self) -> Token:
        start_index = self.index
        start_column = self.column
        while self.index < self.length and (self.source[self.index].isalnum() or self.source[self.index] == "_"):
            self.index += 1
            self.column += 1
        value = self.source[start_index:self.index]
        type_ = "KEYWORD" if value in self.KEYWORDS else "IDENT"
        start = SourceLocation(self.line, start_column)
        end = SourceLocation(self.line, self.column - 1)
        return Token(type_, value, SourceSpan(start, end))


class Parser:
    def __init__(self, tokens: Iterable[Token]):
        self.tokens: List[Token] = list(tokens)
        self.index = 0
        self.diagnostics: List[Diagnostic] = []

    def parse(self) -> ParseResult:
        program = self._parse_program()
        return ParseResult(program, self.diagnostics)

    def _parse_program(self) -> Optional[Program]:
        self._skip_newlines()
        start = self._current().span.start
        if not self._match_keyword("начало"):
            self._report_unknown(self._current())
            return None
        if not self._match("COLON"):
            self._report_unknown(self._previous())
        self._consume_newline(optional=True)
        statements = self._parse_statements({"конец"})
        end_token = self._current()
        if not self._match_keyword("конец"):
            self._report_mismatch(end_token)
            return Program(statements=statements, span=SourceSpan(start, end_token.span.end))
        end_token = self._previous()
        if self._match("DOT"):
            end_token = self._previous()
        else:
            self._report_unknown(self._current())
        end_span = SourceSpan(start, end_token.span.end)
        return Program(statements=statements, span=end_span)

    def _parse_statements(self, terminators: Iterable[str]) -> List[Node]:
        statements: List[Node] = []
        terminators_set = set(terminators)
        while not self._check("EOF"):
            current = self._current()
            if current.type == "NEWLINE":
                self._advance()
                continue
            if current.type == "KEYWORD" and current.value in terminators_set:
                break
            statement = self._parse_statement()
            if statement is None:
                self._synchronize()
            else:
                statements.append(statement)
            self._consume_newline(optional=True)
        return statements

    def _parse_statement(self) -> Optional[Node]:
        token = self._current()
        if token.type != "KEYWORD":
            self._report_unknown(token)
            return None
        if token.value == "показать":
            return self._parse_show()
        if token.value == "переменная":
            return self._parse_variable()
        if token.value == "обучить":
            return self._parse_teach()
        if token.value == "создать":
            return self._parse_create_formula()
        if token.value == "оценить":
            return self._parse_evaluate()
        if token.value == "если":
            return self._parse_if()
        if token.value == "пока":
            return self._parse_while()
        if token.value == "сохранить":
            return self._parse_save()
        if token.value == "отбросить":
            return self._parse_drop()
        if token.value == "вызвать":
            return self._parse_call_evolution()
        if token.value == "распечатать":
            return self._parse_print_canvas()
        if token.value == "рой":
            return self._parse_swarm_send()
        self._report_unknown(token)
        return None

    def _parse_show(self) -> Optional[Node]:
        start = self._advance().span.start
        value = self._parse_expression_until({"NEWLINE", "EOF"})
        span = SourceSpan(start, value.span.end if value else self._previous().span.end)
        if value is None:
            self._report_unknown(self._current())
            return None
        return ShowStatement(span=span, value=value)

    def _parse_variable(self) -> Optional[Node]:
        start_token = self._advance()
        name_token = self._expect("IDENT")
        if name_token is None:
            return None
        if not self._match("ASSIGN"):
            self._report_unknown(self._current())
            return None
        value = self._parse_expression_until({"NEWLINE", "EOF"})
        if value is None:
            self._report_unknown(self._current())
            return None
        span = SourceSpan(start_token.span.start, value.span.end)
        return VariableDeclaration(span=span, name=name_token.value, value=value)

    def _parse_teach(self) -> Optional[Node]:
        start_token = self._advance()
        if not self._match_keyword("связь"):
            self._report_unknown(self._current())
            return None
        left = self._parse_expression_until({"ARROW"})
        if left is None or not self._match("ARROW"):
            self._report_unknown(self._current())
            return None
        right = self._parse_expression_until({"NEWLINE", "EOF"})
        if right is None:
            self._report_unknown(self._current())
            return None
        span = SourceSpan(start_token.span.start, right.span.end)
        return TeachAssociation(span=span, left=left, right=right)

    def _parse_create_formula(self) -> Optional[Node]:
        start_token = self._advance()
        if not self._match_keyword("формулу"):
            self._report_unknown(self._current())
            return None
        name_token = self._expect("IDENT")
        if name_token is None:
            return None
        if not self._match_keyword("из"):
            self._report_unknown(self._current())
            return None
        expression = self._parse_expression_until({"NEWLINE", "EOF"})
        if expression is None:
            self._report_unknown(self._current())
            return None
        span = SourceSpan(start_token.span.start, expression.span.end)
        return CreateFormula(span=span, name=name_token.value, expression=expression)

    def _parse_evaluate(self) -> Optional[Node]:
        start_token = self._advance()
        name_token = self._expect("IDENT")
        if name_token is None:
            return None
        if not self._match_keyword("на") or not self._match_keyword("задаче"):
            self._report_unknown(self._current())
            return None
        task = self._parse_expression_until({"NEWLINE", "EOF"})
        if task is None:
            self._report_unknown(self._current())
            return None
        span = SourceSpan(start_token.span.start, task.span.end)
        return EvaluateFormula(span=span, name=name_token.value, task=task)

    def _parse_if(self) -> Optional[Node]:
        start_token = self._advance()
        condition = self._parse_expression_until({"KEYWORD:тогда"})
        if condition is None or not self._match_keyword("тогда"):
            self._report_unknown(self._current())
            return None
        self._consume_newline(optional=True)
        then_body = self._parse_statements({"иначе", "конец"})
        else_body: List[Node] = []
        if self._match_keyword("иначе"):
            self._consume_newline(optional=True)
            else_body = self._parse_statements({"конец"})
        if not self._match_keyword("конец"):
            self._report_mismatch(start_token)
            end_span = (then_body[-1].span if then_body else start_token.span).end
            return IfStatement(
                span=SourceSpan(start_token.span.start, end_span),
                condition=condition or Expression("", start_token.span),
                then_body=then_body,
                else_body=else_body,
            )
        if self._check("DOT"):
            self._report_mismatch(start_token)
        span = SourceSpan(start_token.span.start, self._previous().span.end)
        return IfStatement(span=span, condition=condition, then_body=then_body, else_body=else_body)

    def _parse_while(self) -> Optional[Node]:
        start_token = self._advance()
        condition = self._parse_expression_until({"KEYWORD:делать"})
        if condition is None or not self._match_keyword("делать"):
            self._report_unknown(self._current())
            return None
        self._consume_newline(optional=True)
        body = self._parse_statements({"конец"})
        if not self._match_keyword("конец"):
            self._report_mismatch(start_token)
            end_span = (body[-1].span if body else start_token.span).end
            return WhileStatement(
                span=SourceSpan(start_token.span.start, end_span),
                condition=condition or Expression("", start_token.span),
                body=body,
            )
        if self._check("DOT"):
            self._report_mismatch(start_token)
        span = SourceSpan(start_token.span.start, self._previous().span.end)
        return WhileStatement(span=span, condition=condition, body=body)

    def _parse_save(self) -> Optional[Node]:
        start_token = self._advance()
        name_token = self._expect("IDENT")
        if name_token is None:
            return None
        if not self._match_keyword("в") or not self._match_keyword("геном"):
            self._report_unknown(self._current())
            return None
        return SaveFormula(span=SourceSpan(start_token.span.start, self._previous().span.end), name=name_token.value)

    def _parse_drop(self) -> Optional[Node]:
        start_token = self._advance()
        name_token = self._expect("IDENT")
        if name_token is None:
            return None
        return DropFormula(span=SourceSpan(start_token.span.start, name_token.span.end), name=name_token.value)

    def _parse_call_evolution(self) -> Optional[Node]:
        start_token = self._advance()
        if not self._match_keyword("эволюцию"):
            self._report_unknown(self._current())
            return None
        return CallEvolution(span=SourceSpan(start_token.span.start, self._previous().span.end))

    def _parse_print_canvas(self) -> Optional[Node]:
        start_token = self._advance()
        if not self._match_keyword("канву"):
            self._report_unknown(self._current())
            return None
        return PrintCanvas(span=SourceSpan(start_token.span.start, self._previous().span.end))

    def _parse_swarm_send(self) -> Optional[Node]:
        start_token = self._advance()
        if not self._match_keyword("отправить"):
            self._report_unknown(self._current())
            return None
        name_token = self._expect("IDENT")
        if name_token is None:
            return None
        return SwarmSend(span=SourceSpan(start_token.span.start, name_token.span.end), name=name_token.value)

    def _parse_expression_until(self, terminators: Iterable[str]) -> Optional[Expression]:
        terms = set(terminators)
        parts: List[str] = []
        start_token: Optional[Token] = None
        last_token: Optional[Token] = None
        while True:
            token = self._current()
            key = token.type if token.type != "KEYWORD" else f"KEYWORD:{token.value}"
            if key in terms or token.type == "EOF" or token.type == "NEWLINE":
                break
            parts.append(token.value)
            if start_token is None:
                start_token = token
            last_token = token
            self._advance()
        if not parts:
            return None
        assert start_token is not None and last_token is not None
        span = SourceSpan(start_token.span.start, last_token.span.end)
        return Expression(" ".join(parts), span)

    def _match(self, token_type: str) -> bool:
        if self._check(token_type):
            self._advance()
            return True
        return False

    def _match_keyword(self, value: str) -> bool:
        if self._check("KEYWORD") and self._current().value == value:
            self._advance()
            return True
        return False

    def _expect(self, token_type: str) -> Optional[Token]:
        if self._check(token_type):
            return self._advance()
        self._report_unknown(self._current())
        return None

    def _check(self, token_type: str) -> bool:
        return self._current().type == token_type

    def _current(self) -> Token:
        return self.tokens[self.index]

    def _previous(self) -> Token:
        return self.tokens[self.index - 1]

    def _advance(self) -> Token:
        token = self.tokens[self.index]
        if self.index < len(self.tokens) - 1:
            self.index += 1
        return token

    def _consume_newline(self, *, optional: bool = False) -> None:
        if self._check("NEWLINE"):
            self._advance()
        elif not optional:
            self._report_unknown(self._current())

    def _skip_newlines(self) -> None:
        while self._check("NEWLINE"):
            self._advance()

    def _synchronize(self) -> None:
        while not self._check("EOF") and not self._check("NEWLINE"):
            self._advance()

    def _report_unknown(self, token: Token) -> None:
        message = "Неизвестная команда или идентификатор"
        self.diagnostics.append(Diagnostic(1, message, token.span))

    def _report_mismatch(self, token: Token) -> None:
        message = "Несогласованность блоков `если/конец`"
        self.diagnostics.append(Diagnostic(2, message, token.span))


def parse_script(source: str) -> ParseResult:
    lexer = Lexer(source)
    parser = Parser(lexer)
    return parser.parse()
