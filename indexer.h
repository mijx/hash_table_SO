#ifndef INDEXER_H
#define INDEXER_H

// Incrementar significativamente el tamaño de la tabla hash
// Con 96M registros, necesitas al menos 10M buckets para performance decente
#define HASH_TABLE_SIZE 16777259  // Número primo grande (~16M)

// Estructura para cada nodo en nuestro archivo index.dat
typedef struct {
    long data_offset;      // Posición del registro en dataset.csv
    long next_node_offset; // Posición del siguiente IndexNode en index.dat (-1 si es el final)
} IndexNode;

// Función Hash mejorada para BibNumber + Fecha
// Combina mejor los dos componentes para reducir colisiones
unsigned long hash_function(const char *bibNumber, const char *fecha) {
    unsigned long hash = 5381;
    unsigned long hash2 = 1;
    
    // Procesar BibNumber con hash principal (djb2)
    const char *str = bibNumber;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    // Procesar fecha con segundo hash (multiplicativo)
    str = fecha;
    while ((c = *str++)) {
        hash2 = hash2 * 31 + c;
    }
    
    // Combinar los dos hashes de manera más distribuida
    // Usar XOR y rotación para mejor distribución
    return hash ^ (hash2 << 16) ^ (hash2 >> 16);
}

#endif // INDEXER_H