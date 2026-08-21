"""Interface Streamlit para testar o analisador léxico do MINIC.

Uso:
    python -m streamlit run src/python/app_streamlit.py

Reaproveita o `Lexer` de lexer.py — essa tela é só uma forma alternativa de
visualizar o mesmo resultado que `main.py` imprime no terminal.
"""

import sys
from pathlib import Path

import streamlit as st

sys.path.insert(0, str(Path(__file__).resolve().parent))

from lexer import TOKENS_COM_ATRIBUTO, Lexer  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
INPUTS_DIR = ROOT / "tests" / "inputs"

st.set_page_config(page_title="MINIC Lexer", page_icon="🔤", layout="wide")

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

st.markdown("### 🔤 Analisador léxico do MINIC")
st.caption("Interface de testes sobre `src/python/lexer.py` — a mesma lógica que `main.py` roda no terminal.")

exemplos = sorted(p.name for p in INPUTS_DIR.glob("*.mc")) if INPUTS_DIR.exists() else []

with st.sidebar:
    st.markdown("#### Entrada")
    modo = st.radio(
        "Fonte do código",
        ["Exemplo de tests/inputs", "Colar código", "Upload"],
        label_visibility="collapsed",
    )

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

# `text_area` com `key` ignora `value` em reruns; só sobrescreve o estado
# quando a fonte selecionada de fato muda, senão edições manuais se perdem.
if st.session_state.get("_fonte_id") != fonte_id:
    st.session_state["_fonte_id"] = fonte_id
    st.session_state["codigo"] = codigo_fonte or ""

with st.container(border=True):
    fonte = st.text_area("Código MINIC", height=260, key="codigo")

if not fonte.strip():
    st.info("Escolha um exemplo, faça upload ou cole um código MINIC para analisar.")
    st.stop()

tokens, errors = Lexer(fonte).tokenize()
tokens_exibidos = [t for t in tokens if t.type != "EOF"]
exit_code = 2 if errors else 0

col1, col2, col3 = st.columns(3)
col1.metric("Tokens reconhecidos", len(tokens_exibidos))
col2.metric("Erros léxicos", len(errors))
col3.metric(
    "Código de saída",
    exit_code,
    delta="ok" if exit_code == 0 else "erro léxico",
    delta_color="normal" if exit_code == 0 else "inverse",
)

st.write("")
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
            st.error(str(err))
    else:
        st.success("Nenhum erro léxico — código válido.")

with st.expander("Saída bruta (igual ao `python main.py arquivo.mc`)"):
    st.code("\n".join(str(t) for t in tokens_exibidos) or " ", language=None)
    if errors:
        st.code("\n".join(str(e) for e in errors), language=None)
