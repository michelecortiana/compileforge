  export interface CompileRequest {
      source_code: string;
      language: string;
  }
  
  export interface CompileResponse {
    job_id: string;
  }
  
  export interface ErrorResponse {
    error: string;
  }