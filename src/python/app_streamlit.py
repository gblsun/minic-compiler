"""Interface Streamlit para testar o analisador léxico do MINIC.

Uso:
    python -m streamlit run src/python/app_streamlit.py

Reaproveita o `Lexer` de lexer.py — essa tela é só uma forma alternativa de
visualizar o mesmo resultado que `main.py` imprime no terminal. Nenhuma regra
léxica mora aqui: este arquivo só lê a entrada do usuário, chama o mesmo
`Lexer` da CLI e desenha o resultado. Se o comportamento do lexer mudar,
tanto o terminal quanto esta interface mudam juntos, sem duplicar lógica.
"""

import sys
from pathlib import Path

import streamlit as st

# Quando o Streamlit executa este arquivo, ele não sabe que `lexer.py` está
# na mesma pasta — não estamos rodando como um pacote Python instalado, só
# um script solto. Por isso adicionamos a própria pasta ao `sys.path` antes
# de importar, senão o `import lexer` abaixo falharia com ModuleNotFoundError.
sys.path.insert(0, str(Path(__file__).resolve().parent))

from lexer import TOKENS_COM_ATRIBUTO, Lexer  # noqa: E402  (import depois do sys.path.insert, de propósito)

ROOT = Path(__file__).resolve().parents[2]
INPUTS_DIR = ROOT / "tests" / "inputs"

st.set_page_config(page_title="MINIC Lexer", page_icon="🔤", layout="wide")

# Ajustes visuais que o `[theme]` do .streamlit/config.toml não cobre:
# tipografia (Google Fonts, carregada via @import — funciona porque isso
# roda num navegador comum com internet, não numa sandbox restrita), uma
# largura máxima de conteúdo para não esticar demais em telas largas, e um
# estilo de "card" para os st.metric (que por padrão vêm sem fundo nenhum).
st.markdown(
    """
    <style>
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap');

    html, body, [class*="css"] { font-family: 'Inter', sans-serif; }
    .stTextArea textarea, code, pre { font-family: 'JetBrains Mono', monospace !important; }

    .block-container { max-width: 1100px; padding-top: 3rem; padding-bottom: 3rem; }

    [data-testid="stMetric"] {
        background: var(--secondary-background-color, #f4f5f7);
        border-radius: 12px;
        padding: 0.9rem 1.1rem;
    }
    [data-testid="stMetricLabel"] { font-size: 0.8rem; opacity: 0.65; }
    </style>
    """,
    unsafe_allow_html=True,
)

st.markdown("### Analisador léxico do MINIC")
st.caption("Interface de testes sobre `src/python/lexer.py` — a mesma lógica que `main.py` roda no terminal.")

# Lista de exemplos prontos já usados pela suíte de testes (tests/inputs/) —
# reaproveitar esses arquivos evita manter uma segunda cópia de exemplos só
# para a UI, e garante que "o exemplo que a interface mostra" é sempre o
# mesmo que os testes automatizados validam.
exemplos = sorted(p.name for p in INPUTS_DIR.glob("*.mc")) if INPUTS_DIR.exists() else []

with st.sidebar:
    st.markdown("#### Entrada")
    modo = st.radio(
        "Fonte do código",
        ["Exemplo de tests/inputs", "Colar código", "Upload"],
        label_visibility="collapsed",
    )

    # `fonte_id` identifica de forma única "de onde veio o código atual"
    # (qual exemplo, ou qual arquivo de upload). É usado logo abaixo para
    # decidir se o conteúdo do editor precisa ser trocado.
    fonte_id = "colar"
    codigo_fonte = None
    if modo == "Exemplo de tests/inputs" and exemplos:
        escolhido = st.selectbox("Arquivo de exemplo", exemplos)
        fonte_id = f"exemplo:{escolhido}"
        codigo_fonte = (INPUTS_DIR / escolhido).read_text(encoding="utf-8")
    elif modo == "Upload":
        arquivo = st.file_uploader("Arquivo .mc", type=["mc"])
        if arquivo is not None:
            fonte_id = f"upload:{arquivo.name}:{arquivo.size}"
            codigo_fonte = arquivo.read().decode("utf-8")

# Pegadinha do Streamlit: um widget com `key` guarda seu próprio valor em
# `st.session_state` e, a partir da segunda execução, IGNORA o parâmetro
# `value` — ele confia no que já está salvo naquela chave. Se a gente
# simplesmente passasse `value=codigo_fonte` toda vez, trocar o exemplo no
# selectbox não atualizaria o texto exibido (foi exatamente o bug visto ao
# testar a primeira versão desta tela). A solução é gerenciar o
# session_state manualmente: só sobrescrevemos "codigo" quando a fonte
# realmente muda (comparando `fonte_id` com a última usada); assim, se o
# usuário estiver só editando o texto colado à mão, não pisamos no que ele
# digitou a cada rerender.
if st.session_state.get("_fonte_id") != fonte_id:
    st.session_state["_fonte_id"] = fonte_id
    st.session_state["codigo"] = codigo_fonte or ""

with st.container(border=True):
    fonte = st.text_area("Código MINIC", height=260, key="codigo")

if not fonte.strip():
    st.info("Escolha um exemplo, faça upload ou cole um código MINIC para analisar.")
    st.stop()

# A partir daqui é o mesmo par (tokens, errors) que `main.py` produz — só
# muda a forma de exibir. O token EOF, de novo, é filtrado por ser um
# detalhe interno do lexer.
tokens, errors = Lexer(fonte).tokenize()
tokens_exibidos = [t for t in tokens if t.type != "EOF"]
exit_code = 2 if errors else 0

col1, col2, col3 = st.columns(3)
col1.metric("Tokens reconhecidos", len(tokens_exibidos))
col2.metric("Erros léxicos", len(errors))
col3.metric(
    "Código de saída",
    exit_code,
    # `st.metric` não tem um parâmetro de "cor" direto, mas o `delta`
    # colore automaticamente (verde/vermelho) conforme `delta_color`. Aqui
    # usamos esse comportamento só para dar uma pista visual de status —
    # não representa uma variação numérica de verdade.
    delta="ok" if exit_code == 0 else "erro léxico",
    delta_color="normal" if exit_code == 0 else "inverse",
)

st.write("")
# Abas em vez de duas colunas lado a lado: com muitos tokens ou muitos
# erros, colunas fixas de metade da tela ficam apertadas rápido. Abas dão
# a largura inteira para o que estiver sendo olhado no momento.
aba_tokens, aba_erros = st.tabs([f"Tokens ({len(tokens_exibidos)})", f"Erros léxicos ({len(errors)})"])

with aba_tokens:
    if tokens_exibidos:
        st.dataframe(
            [
                {
                    "tipo": t.type,
                    "lexema": t.lexeme if t.type in TOKENS_COM_ATRIBUTO else "",
                    "linha": t.line,
                    "coluna": t.column,
                }
                for t in tokens_exibidos
            ],
            hide_index=True,
            use_container_width=True,
            # `column_config` só troca os rótulos das colunas (de "tipo"
            # para "Tipo" etc.) e ajusta largura — os dados continuam vindo
            # das chaves em minúsculo do dicionário acima.
            column_config={
                "tipo": st.column_config.TextColumn("Tipo"),
                "lexema": st.column_config.TextColumn("Lexema"),
                "linha": st.column_config.NumberColumn("Linha", width="small"),
                "coluna": st.column_config.NumberColumn("Coluna", width="small"),
            },
        )
    else:
        st.write("Nenhum token reconhecido.")

with aba_erros:
    if errors:
        for err in errors:
            # `str(err)` usa o `__str__` de LexError, então a mensagem é
            # idêntica à que aparece no stderr do `main.py`.
            st.error(str(err))
    else:
        st.success("Nenhum erro léxico — código válido.")

# Painel "modo depuração": mostra exatamente o que apareceria no terminal,
# útil para conferir que a interface e a CLI concordam byte a byte.
with st.expander("Saída bruta (igual ao `python main.py arquivo.mc`)"):
    st.code("\n".join(str(t) for t in tokens_exibidos) or " ", language=None)
    if errors:
        st.code("\n".join(str(e) for e in errors), language=None)
