import type { CompileStatus, ThemeMode } from '../App';

interface NavbarProps {
  onCompile: () => void;
  status: CompileStatus;
  theme: ThemeMode;
  onToggleTheme: () => void;
  onSelectSnippet: (code: string) => void;
  onOpenHistory: () => void;
  onOpenInfo: () => void;
}

const CODE_SNIPPETS = [
  {
    label: 'Somma Semplice',
    code: `int main() {\n    int a = 5;\n    int b = 10;\n    return a + b;\n}`
  },
  {
    label: 'Moltiplicazione',
    code: `int main() {\n    int x = 4;\n    int y = 6;\n    return x * y;\n}`
  },
  {
    label: 'Espansione Variabili',
    code: `int main() {\n    int a = 2;\n    int b = 3;\n    int c = 4;\n    return a + b * c;\n}`
  }
];

export default function Navbar({ onCompile, status, theme, onToggleTheme, onSelectSnippet, onOpenHistory, onOpenInfo }: NavbarProps) {
  const isWorking = status === 'submitting' || status === 'pending' || status === 'processing' || status === 'connected'; 
  const isDark = theme === 'dark';

  return (
    <div className={`flex justify-between items-center p-4 border-b ${isDark ? 'bg-gray-800 border-gray-700 text-white' : 'bg-white border-gray-200 text-gray-800'}`}>
      
      <div className="flex items-center gap-6">
        <h1 className="text-xl font-bold text-blue-500">CompileForge</h1>
        
        <div className="flex items-center gap-2">
          <span className={`text-xs font-medium uppercase tracking-wider ${isDark ? 'text-gray-400' : 'text-gray-500'}`}>Snippet:</span>
          <select 
            onChange={(e) => {
              if (e.target.value !== "") {
                onSelectSnippet(e.target.value);
                e.target.value = ""; 
              }
            }}
            defaultValue=""
            className={`text-xs px-2 py-1.5 rounded border outline-none cursor-pointer transition-colors ${
              isDark 
                ? 'bg-gray-700 border-gray-600 text-gray-200 hover:bg-gray-600' 
                : 'bg-gray-50 border-gray-300 text-gray-700 hover:bg-gray-100'
            }`}
          >
            <option value="" disabled>Seleziona esempio...</option>
            {CODE_SNIPPETS.map((snippet, idx) => (
              <option key={idx} value={snippet.code}>
                {snippet.label}
              </option>
            ))}
          </select>
        </div>
      </div>
      
      <div className="flex items-center gap-3">
        <button
          onClick={onOpenInfo}
          className={`px-3 py-1.5 text-sm font-semibold rounded transition-colors border ${
            isDark 
              ? 'bg-gray-700 border-gray-600 text-amber-400 hover:bg-gray-600' 
              : 'bg-white border-gray-300 text-amber-600 hover:bg-amber-50 shadow-sm'
          }`}
        >
          ℹ️ Info Compilatore
        </button>

        <button
          onClick={onOpenHistory}
          className={`px-3 py-1.5 text-sm font-semibold rounded transition-colors border ${
            isDark 
              ? 'bg-gray-700 border-gray-600 text-gray-200 hover:bg-gray-600' 
              : 'bg-white border-gray-300 text-gray-700 hover:bg-gray-100 shadow-sm'
          }`}
        >
          🕒 Storico Job
        </button>

        <button
          onClick={onToggleTheme}
          title="Cambia Tema"
          className={`p-2 rounded-full border transition-colors ${
            isDark 
              ? 'bg-gray-700 border-gray-600 text-yellow-400 hover:bg-gray-600' 
              : 'bg-gray-100 border-gray-300 text-yellow-600 hover:bg-gray-200'
          }`}
        >
          {isDark ? '☀️' : '🌙'}
        </button>

        <button 
          onClick={onCompile} 
          disabled={isWorking} 
          className={`px-4 py-2 text-sm font-semibold rounded transition-colors ${
            isWorking 
              ? 'bg-gray-500 text-gray-300 cursor-not-allowed' 
              : 'bg-blue-600 hover:bg-blue-500 text-white'
          }`}
        >
          {isWorking ? 'Elaborazione in corso...' : 'Compila ed Esegui'}
        </button>
      </div>
    </div>
  );
}