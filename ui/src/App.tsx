import { useState } from 'react';
import Navbar from './components/Navbar';
import EditorPanel from './components/EditorPanel';
import OutputPanel from './components/OutputPanel';

function App() {
  // Stato globale per il codice C
  const [code, setCode] = useState(`int main() {\n    int a = 5;\n    int b = 10;\n    return a + b;\n}`);

  return (
    <div className="h-screen w-screen flex flex-col bg-gray-900 text-white overflow-hidden">
      {/* Passiamo il codice alla Navbar per la futura fase di invio al backend */}
      <Navbar code={code} />
      
      <div className="flex flex-1 overflow-hidden">
        <div className="w-1/2 border-r border-gray-700">
          {/* Passiamo il codice e la funzione per aggiornarlo al pannello dell'editor */}
          <EditorPanel code={code} setCode={setCode} />
        </div>
        <div className="w-1/2">
          <OutputPanel />
        </div>
      </div>
    </div>
  );
}

export default App;