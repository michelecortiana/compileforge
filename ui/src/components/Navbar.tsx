interface NavbarProps {
  code?: string;
}

const Navbar = ({ code }: NavbarProps) => {
  return (
    <div className="flex justify-between items-center px-6 py-3 bg-gray-800 border-b border-gray-700">
      <h1 className="text-xl font-bold text-white">CompileForge</h1>
      <button 
        onClick={() => console.log("Codice pronto per la compilazione:\n", code)}
        className="px-4 py-2 bg-blue-600 hover:bg-blue-500 text-white font-semibold rounded shadow-sm transition-colors"
      >
        Compile
      </button>
    </div>
  );
};

export default Navbar;