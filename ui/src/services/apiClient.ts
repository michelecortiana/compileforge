// In cima a apiClient.ts, al posto dell'import:
export interface CompileRequest {
  source_code: string;
  language: string;
}

export interface CompileResponse {
  job_id: string;
}
// Recuperiamo le variabili d'ambiente create nella Fase 3.1
// Nota: Vite usa import.meta.env invece del classico process.env
const API_BASE_URL = import.meta.env.VITE_API_BASE_URL;
const API_KEY = import.meta.env.VITE_API_KEY;

export async function submitCode(sourceCode: string): Promise<string> {
  try {
    // 1. Configuriamo e lanciamo la chiamata di rete
    const response = await fetch(`${API_BASE_URL}/compile`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json', // Diciamo al server che stiamo inviando un JSON
        'x-api-key': API_KEY,               // Inseriamo la nostra chiave segreta come richiesto dal Gateway
      },
      body: JSON.stringify({
        source_code: sourceCode,
        language: 'c', // Per ora lo fissiamo a C
      }),
    });

    // 2. Gestione degli errori HTTP (es. 401 Unauthorized, 500 Server Error)
    if (!response.ok) {
      // Proviamo a leggere il messaggio di errore dal backend, se c'è
      const errorData = await response.json().catch(() => ({}));
      throw new Error(errorData.error || `Errore HTTP: ${response.status}`);
    }

    // 3. Estrazione del dato
    // Se la chiamata va a buon fine, convertiamo la risposta in JSON
    // e TypeScript sa già che avrà la forma di CompileResponse
    const data: CompileResponse = await response.json();
    
    // Restituiamo solo l'ID del lavoro, che è l'unica cosa che ci serve ora
    return data.job_id;
    
  } catch (error) {
    console.error("Errore durante la comunicazione col Gateway:", error);
    throw error; // Rilanciamo l'errore per farlo gestire a chi ha chiamato la funzione
  }
}
// Assicurati che ci sia "export" all'inizio!
export interface JobStatusUpdate {
  status: 'pending' | 'processing' | 'completed' | 'failed' | 'connected';
  message?: string;
  output?: string;
  errorMessage?: string;
  assembly?: string;
  irCode?: string;
  createdAt?: string;
  finishedAt?: string;
  downloadUrl?: string; 
}

/**
 * Apre una connessione SSE per ascoltare gli aggiornamenti di un job specifico.
 * 
 * @param jobId L'ID del lavoro da tracciare
 * @param onUpdate Callback invocata ogni volta che arriva un nuovo messaggio dal server
 * @param onError Callback invocata in caso di errore di connessione
 * @returns Una funzione di cleanup per chiudere la connessione manualmente
 */
export function listenForJobStatus(
  jobId: string,
  onUpdate: (data: JobStatusUpdate) => void,
  onError: (error: Event) => void
): () => void {
  // Puntiamo all'endpoint esatto definito nel tuo server.ts
  const streamUrl = `${API_BASE_URL}/status/${jobId}/stream`;
  
  // Apriamo la connessione unidirezionale
  const eventSource = new EventSource(streamUrl);

  // Gestione dei messaggi in arrivo
  eventSource.onmessage = (event) => {
    try {
      const data: JobStatusUpdate = JSON.parse(event.data);
      onUpdate(data);

      // Se riceviamo uno stato finale, il server ha finito. 
      // Chiudiamo la connessione lato client per liberare risorse.
      if (data.status === 'completed' || data.status === 'failed') {
        eventSource.close();
      }
    } catch (err) {
      console.error("Errore nel parsing del messaggio SSE:", err);
    }
  };

  // Gestione degli errori di connessione
  eventSource.onerror = (error) => {
    console.error("Errore nella connessione SSE:", error);
    eventSource.close(); // Chiudiamo per evitare loop di riconnessione impazziti
    onError(error);
  };

  // Restituiamo una funzione che React potrà chiamare nel suo useEffect 
  // per chiudere la connessione se il componente viene smontato
  return () => {
    if (eventSource.readyState !== EventSource.CLOSED) {
      eventSource.close();
    }
  };
}