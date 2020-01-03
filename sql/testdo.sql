do $$
	int sumResult = 3 + 7;
	Console.WriteLine($"Compiled Sum: {sumResult}");
    /* Check if there is a 'Compiled Sum: 10' in PostgreSQL logs */
$$ language plcsharp;

SELECT 1;
