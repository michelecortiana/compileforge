import { useState } from 'react';
import type { CompileStatus, ThemeMode } from '../App';

interface OutputPanelProps {
  status: CompileStatus;
  output: string;
  assembly?: string;
  irCode?: string;
  metrics?: { start?: string; end?: string };
  downloadUrl?: string | null;
  theme: ThemeMode; // 👈 Riceve il tema corrente
}

type TabType = 'terminal' | 'assembly' | 'ir' | 'metrics';

export default function OutputPanel({ status, output, assembly, irCode, metrics, downloadUrl, theme }: OutputPanelProps) {
  const [activeTab, setActiveTab] = useState<TabType>('terminal');
  const isDark = theme === 'dark';

  const stripAnsi = (str: string) => {
    if (!str) return "";
    return str.replace(/\x1b\[[0-9;]*m/g, '');
  };

  const getStatusBadge = () => {
    switch (status) {
      case 'idle': return <span className="text-gray-500">In attesa...</span>;
      case 'submitting': 
      case 'queued': return <span className="text-yellow-400 animate-pulse">In coda...</span>;
      case 'compiling': return <span className="text-blue-400 animate-pulse">Compilazione...</span>;
      case 'completed': return <span className="text-green-500 font-bold">Completato</span>;
      case 'error': return <span className="text-red-500 font-bold">Errore</span>;
      default: return null;
    }
  };

  const calculateDuration = () => {
    if (!metrics?.start || !metrics?.end) return 'N/D';
    const start = new Date(metrics.start).getTime();
    const end = new Date(metrics.end).getTime();
    return `${end - start} ms`;
  };

  const renderTabButton = (id: TabType, label: string) => (
    <button
      onClick={() => setActiveTab(id)}
      className={`px-4 py-2 text-sm font-semibold transition-colors border-b-2 ${
        activeTab === id
          ? isDark ? 'border-blue-500 text-blue-400 bg-gray-800' : 'border-blue-600 text-blue-600 bg-gray-100'
          : isDark ? 'border-transparent text-gray-400 hover:text-gray-200 hover:bg-gray-700' : 'border-transparent text-gray-500 hover:text-gray-800 hover:bg-gray-200'
      }`}
    >
      {label}
    </button>
  );

  return (
    <div className={`flex flex-col h-full border-l ${isDark ? 'bg-[#0d1117] border-gray-700 text-gray-300' : 'bg-white border-gray-200 text-gray-800'}`}>
      
      {/* Header con Badge di stato e Bottone Download */}
      <div className={`flex justify-between items-center px-4 py-2 border-b shadow-sm ${isDark ? 'bg-gray-900 border-gray-700' : 'bg-gray-50 border-gray-200'}`}>
        <div className="flex items-center gap-4">
          <h2 className={`text-sm font-semibold uppercase tracking-wider ${isDark ? 'text-gray-300' : 'text-gray-700'}`}>
            Risultati
          </h2>
          {downloadUrl && status === 'completed' && (
            <a 
              href={downloadUrl} 
              download 
              className="px-3 py-1 bg-blue-600 hover:bg-blue-500 text-white text-xs font-bold rounded shadow transition-colors"
            >
              📥 Scarica Eseguibile (Linux x86-64)
            </a>
          )}
        </div>
        <div className={`text-xs font-mono px-2 py-1 rounded border ${isDark ? 'bg-black border-gray-700 text-white' : 'bg-gray-200 border-gray-300 text-gray-900'}`}>
          Stato: {getStatusBadge()}
        </div>
      </div>
      
      {/* Barra di navigazione dei Tab */}
      <div className={`flex border-b ${isDark ? 'bg-gray-900 border-gray-700' : 'bg-gray-100 border-gray-200'}`}>
        {renderTabButton('terminal', 'Terminale')}
        {renderTabButton('ir', 'IR Code')}
        {renderTabButton('assembly', 'Assembly (x86-64)')}
        {renderTabButton('metrics', 'Metriche')}
      </div>
      
      {/* Area del contenuto */}
      <div className="flex-1 p-4 overflow-y-auto font-mono text-sm">
        
        {activeTab === 'terminal' && (
          output ? (
            <pre className={`whitespace-pre-wrap break-words ${status === 'error' ? 'text-red-400' : (isDark ? 'text-gray-300' : 'text-gray-800')}`}>
              {stripAnsi(output)}
            </pre>
          ) : (
            <p className="text-gray-500 italic">Nessun output da mostrare.</p>
          )
        )}

        {activeTab === 'ir' && (
          irCode ? (
            <pre className={isDark ? "text-green-300 whitespace-pre-wrap" : "text-green-700 whitespace-pre-wrap"}>{irCode}</pre>
          ) : (
            <p className="text-gray-500 italic">Il codice Intermedio (IR) non è ancora disponibile.</p>
          )
        )}

        {activeTab === 'assembly' && (
          assembly ? (
            <pre className={isDark ? "text-yellow-300 whitespace-pre-wrap" : "text-amber-700 whitespace-pre-wrap"}>{assembly}</pre>
          ) : (
            <p className="text-gray-500 italic">Il codice Assembly non è ancora disponibile.</p>
          )
        )}

        {activeTab === 'metrics' && (
          <div className="flex flex-col gap-4 font-sans">
            <h3 className={`text-lg font-semibold border-b pb-2 ${isDark ? 'text-white border-gray-700' : 'text-gray-900 border-gray-200'}`}>Dettagli Esecuzione</h3>
            <div className="grid grid-cols-2 gap-4">
              <div className={`p-3 rounded border ${isDark ? 'bg-gray-800 border-gray-700 text-gray-300' : 'bg-gray-50 border-gray-200 text-gray-700'}`}>
                <p className="text-xs text-gray-400 uppercase">Tempo di Compilazione</p>
                <p className="text-xl font-mono text-blue-500">{calculateDuration()}</p>
              </div>
              <div className={`p-3 rounded border ${isDark ? 'bg-gray-800 border-gray-700 text-gray-300' : 'bg-gray-50 border-gray-200 text-gray-700'}`}>
                <p className="text-xs text-gray-400 uppercase">Inizio Job</p>
                <p className="text-sm font-mono">{metrics?.start ? new Date(metrics.start).toLocaleTimeString() : 'N/D'}</p>
              </div>
            </div>
          </div>
        )}

      </div>
    </div>
  );
}