#!/usr/bin/env python
"""
Script para visualizar o banco de dados SQLite em formato de tabela.
Uso: python view_database.py
"""

import sqlite3
import sys
from pathlib import Path

def print_table(table_name, cursor):
    """Exibe uma tabela do banco de dados em formato tabular."""
    try:
        cursor.execute(f"SELECT * FROM {table_name}")
        rows = cursor.fetchall()
        
        # Obter nomes das colunas
        columns = [description[0] for description in cursor.description]
        
        # Calcular largura das colunas
        col_widths = []
        for i, col in enumerate(columns):
            max_width = len(col)
            for row in rows:
                max_width = max(max_width, len(str(row[i])))
            col_widths.append(max_width + 2)
        
        # Imprimir cabeçalho
        header = " | ".join(col.ljust(col_widths[i]) for i, col in enumerate(columns))
        print(header)
        print("-" * len(header))
        
        # Imprimir linhas
        if rows:
            for row in rows:
                line = " | ".join(str(val).ljust(col_widths[i]) for i, val in enumerate(row))
                print(line)
        else:
            print("(Tabela vazia)")
        
        print(f"Total: {len(rows)} registro(s)\n")
        
    except Exception as e:
        print(f"❌ Erro ao ler tabela {table_name}: {e}\n")

def main():
    """Função principal."""
    db_path = Path(__file__).parent / "backend" / "database.db"
    
    if not db_path.exists():
        print(f"❌ Banco de dados não encontrado: {db_path}")
        sys.exit(1)
    
    try:
        conn = sqlite3.connect(str(db_path))
        cursor = conn.cursor()
        
        # Obter lista de tabelas
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table';")
        tables = [table[0] for table in cursor.fetchall()]
        
        print("\n" + "=" * 100)
        print(f"📊 BANCO DE DADOS: {db_path}")
        print("=" * 100 + "\n")
        
        if not tables:
            print("Nenhuma tabela encontrada no banco de dados.\n")
        else:
            for table in tables:
                print(f"📋 Tabela: {table}")
                print("-" * 100)
                print_table(table, cursor)
        
        print("=" * 100)
        conn.close()
        
    except sqlite3.Error as e:
        print(f"❌ Erro ao conectar ao banco de dados: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
