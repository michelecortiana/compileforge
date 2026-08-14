import type { ThemeMode } from '../App';

interface LimitationsModalProps {
  onClose: () => void;
  theme: ThemeMode;
}

export default function LimitationsModal({ onClose, theme }: LimitationsModalProps) {
  const isDark = theme === 'dark';

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black bg-opacity-60 backdrop-blur-sm p-4">
      <div className={`relative w-full max-w-4xl max-h-[90vh] overflow-y-auto rounded-xl shadow-2xl p-6 border ${isDark ? 'bg-gray-900 border-gray-700 text-gray-300' : 'bg-white border-gray-200 text-gray-800'}`}>
        
        {/* Tasto X in alto a destra */}
        <button
          onClick={onClose}
          className={`absolute top-4 right-4 w-8 h-8 flex items-center justify-center rounded-full font-bold transition-transform hover:scale-110 ${isDark ? 'bg-gray-800 text-gray-400 hover:text-white' : 'bg-gray-100 text-gray-500 hover:text-black'}`}
          title="Chiudi"
        >
          ✕
        </button>

        <h2 className="text-2xl font-bold text-amber-500 mb-2">⚠️ Limitazioni del Compilatore</h2>
        <p className="text-sm italic mb-6 border-b pb-4 border-gray-500">
          Questo compilatore è un progetto didattico e possiede un'architettura core robusta, ma non implementa tutte le funzionalità dello standard ISO C.
        </p>

        <div className="space-y-6 text-sm">
          {/* Sezione 1 */}
          <section>
            <h3 className={`text-lg font-bold mb-2 ${isDark ? 'text-blue-400' : 'text-blue-600'}`}>1. Preprocessor & Toolchain</h3>
            <ul className="list-disc pl-5 space-y-1">
              <li><strong className={isDark ? 'text-gray-200' : 'text-black'}>Nessun Preprocessore:</strong> Zero supporto per <code>#include</code>, <code>#define</code>, <code>#ifdef</code>, o <code>#pragma</code>. Il lexer non riconosce il carattere <code>#</code>.</li>
              <li><strong className={isDark ? 'text-gray-200' : 'text-black'}>Solo Commenti Singoli:</strong> Supportati solo i commenti <code>//</code>. I commenti multilinea (<code>/* ... */</code>) causeranno errori sintattici.</li>
              <li><strong className={isDark ? 'text-gray-200' : 'text-black'}>No Custom libc & Dipendenze Esterne:</strong> Funzioni come <code>printf</code>, <code>malloc</code>, e <code>free</code> sono trattate come keyword native. Il compilatore genera testo assembly ma si affida a GCC/binutils di sistema per l'assemblaggio finale.</li>
              <li><strong className={isDark ? 'text-gray-200' : 'text-black'}>Singolo File:</strong> Nessun supporto per header files o per linkare multipli file <code>.c</code>.</li>
            </ul>
          </section>

          {/* Sezione 2 */}
          <section>
            <h3 className={`text-lg font-bold mb-2 ${isDark ? 'text-blue-400' : 'text-blue-600'}`}>2. Types & Language Syntax</h3>
            <ul className="list-disc pl-5 space-y-1">
              <li><strong className={isDark ? 'text-gray-200' : 'text-black'}>Tipi e Qualificatori:</strong> Solo tipi signed (<code>char</code>, <code>int</code>, <code>float</code>, etc.). Niente varianti unsigned. Keyword come <code>const</code>, <code>static</code>, <code>volatile</code>, <code>enum</code>, <code>typedef</code> e <code>union</code> non sono implementate.</li>
              <li><strong className={isDark ? 'text-gray-200' : 'text-black'}>No Booleans:</strong> Le keyword <code>_Bool</code>, <code>true</code> e <code>false</code> sono assenti.</li>
              <li><strong className={isDark ? 'text-gray-200' : 'text-black'}>Array 1D e Puntatori:</strong> Non supportati array multidimensionali o puntatori a funzione.</li>
              <li><strong className={isDark ? 'text-gray-200' : 'text-black'}>Operatori Mancanti:</strong> NOT logico standalone (<code>!</code>), operatori bit a bit (<code>^</code>, <code>&lt;&lt;</code>, <code>&gt;&gt;</code>), assegnazioni composte (<code>+=</code>, <code>++</code>) e operatore ternario (<code>?:</code>) non sono supportati.</li>
              <li><strong className={isDark ? 'text-gray-200' : 'text-black'}>Controllo di Flusso Limitato:</strong> Niente <code>switch/case</code>, <code>do-while</code>, o <code>goto/labels</code>.</li>
              <li><strong className={isDark ? 'text-gray-200' : 'text-black'}>Scoping e Dichiarazioni:</strong> Symbol table piatta (no block-level shadowing). Niente variabili globali o dichiarazioni multiple sulla stessa riga. Nessuna forward declaration (le funzioni vanno definite prima dell'uso).</li>
              <li><strong className={isDark ? 'text-gray-200' : 'text-black'}>Chiamate a Funzione:</strong> Una funzione deve sempre ritornare un valore supportato (niente <code>void</code>) e la chiamata deve essere assegnata (es. <code>int r = foo();</code>), mai usata come statement puro.</li>
            </ul>
          </section>

          {/* Sezione 3 */}
          <section>
            <h3 className={`text-lg font-bold mb-2 ${isDark ? 'text-blue-400' : 'text-blue-600'}`}>3. Structs, Memory & ABI (x86-64 Linux)</h3>
            <ul className="list-disc pl-5 space-y-1">
              <li><strong className={isDark ? 'text-gray-200' : 'text-black'}>Limiti Statici:</strong> Massimo 20 campi per struct, 50 struct totali, 100 simboli per funzione.</li>
              <li><strong className={isDark ? 'text-gray-200' : 'text-black'}>No Register Allocation & ABI:</strong> Ogni variabile temporanea IR ha uno slot fisso da 8-byte nello stack. Le struct sono passate per valore tramite pointer nascosto (incompatibile con SysV ABI standard).</li>
              <li><strong className={isDark ? 'text-gray-200' : 'text-black'}>Sicurezza e Debug:</strong> Compilazione con <code>-no-pie</code>. Nessuna informazione di debug DWARF (gdb non utilizzabile a livello sorgente).</li>
            </ul>
          </section>
        </div>

        <div className="mt-8 flex justify-end">
          <button
            onClick={onClose}
            className="px-6 py-2 bg-blue-600 hover:bg-blue-500 text-white font-bold rounded shadow transition-colors"
          >
            Ho capito, chiudi
          </button>
        </div>

      </div>
    </div>
  );
}