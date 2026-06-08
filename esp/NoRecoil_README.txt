═══════════════════════════════════════════════════════════════
                    NO RECOIL IMPLEMENTATION
═══════════════════════════════════════════════════════════════

OFFSETS UTILIZADOS (Do arquivo Offsets.hpp):
────────────────────────────────────────────────────────────────

Weapon                = 0x3F8      // Pointer para a arma do jogador
WeaponData            = 0x58       // Dados da arma
WeaponRecoil          = 0xC        // Offset do valor de recoil

COMO FUNCIONA:
────────────────────────────────────────────────────────────────

1. Loop Principal:
   - Verifica se NoRecoil está ativado (AimbotNoRecoil)
   - Obtém endereço do jogador local (cachedLocalPlayer)
   - Lê o ponteiro da arma: localPlayer + 0x3F8
   - Lê os dados da arma: weaponAddr + 0x58
   - Lê o valor de recoil: weaponData + 0xC

2. Armazenamento:
   - Guarda o valor de recoil original em originalRecoilMap
   - Usa weaponData como chave (uint32_t)

3. Ativação:
   - Quando habilitado: escreve 0.0f no offset de recoil
   - Quando desabilitado: restaura o valor original

4. Limpeza:
   - Ao parar, restaura todos os valores originais

INTEGRAÇÃO NO PROJETO:
────────────────────────────────────────────────────────────────

1. Incluir o header:
   #include "esp/NoRecoil.hpp"

2. Chamar no loop principal:
   Exploit::NoRecoil::Work();

3. Na UI/Menu:
   - Checkbox "NoRecoil" já está em "Aimbot Extras"
   - Controla a variável global AimbotNoRecoil
   - NoRecoilStrength pode ser usado para intensidade

ARQUIVOS CRIADOS:
────────────────────────────────────────────────────────────────

✓ NoRecoil.hpp  - Header com definições
✓ NoRecoil.cpp  - Implementação com offsets corretas

THREAD MANAGEMENT:
────────────────────────────────────────────────────────────────

- Thread rodando a 5ms (200 Hz)
- Detached para não bloquear a thread principal
- atomic<bool> para sincronização segura
- unordered_map para armazenar recoil original

PERFORMANCE:
────────────────────────────────────────────────────────────────

- Verificações rápidas por frame
- Memória mínima (apenas map de floats)
- Sleep entre iterações para não sobrecarregar CPU
- Sem impact significativo na performance

═══════════════════════════════════════════════════════════════
