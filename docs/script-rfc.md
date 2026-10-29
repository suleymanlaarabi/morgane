# RFC-0001 – Langage de script de niveau (LVL)

Équipe R-Type · 29 octobre 2026 · Statut : **Brouillon**

## 1. Résumé

Ce document spécifie **LVL** (nom provisoire, extension `.lvl`), un langage destiné à décrire les niveaux de R-Type. Un niveau est un fichier texte que l'on peut éditer à la main ou depuis l'éditeur visuel, sans perte d'information dans les deux sens.

Le langage a deux couches :

- une couche **déclarative** (entités, composants, templates, relations parent-enfant, timeline), que l'éditeur comprend entièrement ;
- une couche **script** (variables, boucles, `await`, fonctions natives), pour la logique que la couche déclarative ne sait pas exprimer. L'éditeur la traite comme un bloc de texte opaque.

## 2. Conventions

Les mots DOIT, NE DOIT PAS, DEVRAIT et PEUT sont à interpréter comme dans la RFC 2119. Les éléments marqués *non normatif* sont des exemples ou des recommandations.

## 3. Motivation et objectifs

**Objectifs**

1. Permettre l'édition visuelle et textuelle du même fichier (round-trip, section 14).
2. Être **déterministe** : serveur et client obtiennent le même niveau à partir du même fichier.
3. Être **vérifiable avant l'exécution** : types, noms et structure sont contrôlés au chargement.
4. Rester petit : un étudiant doit pouvoir lire la spécification en une heure.

**Non-objectifs**

- Ce n'est pas un langage généraliste : pas de fonctions définies dans le langage, pas de récursion, pas de `while`, pas d'entrées/sorties. Les fonctions natives (C++) viennent du registre (section 11.1).
- Le langage ne connaît aucune règle du jeu : fond, défilement, musique et enchaînement des niveaux sont du ressort du jeu (section 9.1).
- Ce n'est pas le langage de l'interface de l'éditeur.
- Cette RFC ne définit pas l'implémentation (parser, VM, intégration à siecs).

## 4. Vue d'ensemble

```
const SCREEN_W = 1920
const SPAWN_X  = SCREEN_W + 64

template Grunt(y: float, hp: int = 3) {
    Sprite:    { path: "bug.png", layer: 2 }
    Transform: { pos: (SPAWN_X, y) }
    Health:    hp
    Movement:  sine(amplitude: 50, period: 2s)
}

level "Stage 1" {
    seed: 42

    Background {
        Sprite:    { path: "space.png" }
        Scrolling: { speed: 2.0 }
    }

    timeline {
        at 2s every 500ms repeat 5 {
            Grunt(y: 100 + 150 * index)
        }

        at 30s {
            Boss() as boss
        }

        when boss.Death {
            emit LevelComplete
        }
    }
}
```

## 5. Structure lexicale

### 5.1 Fichier source

Un fichier est en UTF-8, avec des fins de ligne `LF` ou `CRLF`. Un fichier commence par ses `import` éventuels.

### 5.2 Commentaires

`// jusqu'à la fin de ligne` et `/* bloc, non imbriqué */`. Les commentaires sont conservés par les outils (section 14).

### 5.3 Séparateurs de lignes

Un saut de ligne termine une instruction. Il est ignoré :

- à l'intérieur de `( )` ;
- après une virgule, un opérateur binaire ou une accolade ouvrante ;
- avant une accolade fermante.

Le point-virgule n'existe pas. Plusieurs champs sur une ligne se séparent par des virgules.

### 5.4 Identifiants et casse

Un identifiant respecte `[A-Za-z_][A-Za-z0-9_]*`. La casse est **normative** :

| Forme | Désigne |
| --- | --- |
| `UIdent` (commence par une majuscule) | templates, entités, composants, événements, énumérations |
| `lident` (commence par une minuscule) | variables, champs, paramètres, propriétés de niveau, constructeurs |
| `_` seul | entité anonyme |

Cette règle permet de distinguer sans ambiguïté `Health: 3` (composant) de `scroll_speed: 2.0` (propriété), et un nom de template d'un nom de variable.

### 5.5 Littéraux

| Type | Forme | Exemples |
| --- | --- | --- |
| `int` | chiffres | `0`, `42` |
| `float` | chiffres, point, chiffres | `2.5`, `0.01` |
| `bool` | `true`, `false` |  |
| `string` | guillemets doubles | `"bug.png"` |
| `vec2` | deux expressions numériques entre parenthèses | `(100, 200.5)` |
| `color` | `#` suivi de 6 ou 8 chiffres hexadécimaux (RGB ou RGBA) | `#ff8800`, `#ff8800cc` |
| `duration` | nombre suivi de `ms` ou `s`, sans espace | `500ms`, `2s`, `1.5s` |

Dans une chaîne, les échappements valides sont `\n`, `\t`, `\"`, `\\`, `\{` et `\}`. Une expression entre `{ }` est interpolée : `"enemy{i}"`.

Une durée sans unité (`2`) est une erreur : le langage n'a pas d'unité implicite.

### 5.6 Mots réservés

```
import const template level timeline at every repeat when once count
emit despawn script let mut if else for in loop break await parallel race
branch self parent as and or not true false
```

Les noms de types (`int`, `float`, `bool`, `string`, `vec2`, `color`, `duration`, `entity`) sont aussi réservés.

## 6. Types et expressions

### 6.1 Types

`int` (32 bits signé), `float` (32 bits IEEE 754), `bool`, `string`, `vec2`, `color`, `duration`, `entity` (référence vers une entité), et les **énumérations** fournies par le registre (section 11).

Le typage est statique. La seule conversion implicite est `int` vers `float`. Il n'y a pas de `null`.

### 6.2 Opérateurs

Du moins prioritaire au plus prioritaire :

| Niveau | Opérateurs |
| --- | --- |
| 1 | `or` |
| 2 | `and` |
| 3 | `not` (préfixe) |
| 4 | `==` `!=` `<` `>` `<=` `>=` |
| 5 | `+` `-` |
| 6 | `*` `/` `%` |
| 7 | `-` (préfixe) |
| 8 | accès `.` et appel `()` |

Typage des opérations :

- `int` et `float` : opérations arithmétiques usuelles. `int / int` est une division entière, `%` est réservé aux `int`.
- `vec2` : `vec2 + vec2`, `vec2 - vec2`, `vec2 * nombre`, `vec2 / nombre`.
- `duration` : `duration ± duration`, `duration * nombre`, `duration / nombre`, comparaisons entre durées.
- `string` : `+` concatène. Les comparaisons `==` et `!=` sont permises.
- Une division par zéro est une erreur d'exécution (section 15).

### 6.3 Parenthèses, vec2 et appels

`(a)` est un groupement, `(a, b)` est un `vec2`. Un identifiant suivi de `(` est un appel (section 12).

### 6.4 Intervalles

`a``  to  ``b` est un intervalle d'entiers **semi-ouvert** (`0`` `` to ` `4` donne 0, 1, 2, 3). Il n'est valide que dans une boucle `for`.

### 6.5 Accès aux membres

`ref.Composant.champ` lit un champ d'une entité. `ref` est un nom d'entité, une variable de type `entity`, `self`, `parent`, ou un enfant désigné par `::` (section 8.7). Une lecture sur un composant absent est une erreur d'exécution.

## 7. Structure d'un fichier

Un fichier contient dans cet ordre : les `import`, puis dans n'importe quel ordre des `const`, `template` et `level`.

### 7.1 import

```
import "common/enemies.lvl"
```

Importe les `const` et `template` du fichier visé. Le chemin est relatif au fichier importeur et DOIT rester dans le dossier du projet. Les cycles et les collisions de noms sont des erreurs. Un `level` n'est pas importable.

### 7.2 const

```
const SCREEN_W = 1920
```

Une constante globale. L'expression DOIT être évaluable sans exécuter de script.

## 8. Couche déclarative

### 8.1 Composants

Un composant s'attache à une entité par `Nom: valeur`.

```
Transform: { pos: (1920, 300), rot: 0.0 }   // champs nommés
Health: 3                                   // forme courte
```

- **Champs nommés** : l'ordre est libre, les champs omis prennent la valeur par défaut déclarée par le registre. Un champ inconnu ou dupliqué est une erreur.
- **Forme courte** : permise si le registre déclare un *champ par défaut* pour ce composant. `Health: 3` équivaut à `Health: { max: 3 }` si `max` est ce champ.
- Un même composant NE DOIT PAS apparaître deux fois dans le même corps.

### 8.2 Entités

Une entité est déclarée par un nom suivi d'un corps :

```
Player {
    Transform: { pos: (100, 540) }
    Sprite:    { path: "ship.png" }
}

_ {
    Sprite: { path: "star.png" }
}
```

- Un `UIdent` suivi de `{` déclare une entité **nommée**. Le nom est unique dans le bloc qui la contient et sert à la référencer.
- `_` déclare une entité **anonyme**.
- Une entité déclarée directement dans un `level` existe dès l'instant 0.
- Un corps peut contenir : composants, entités enfants (section 8.7), boucles `for` déclaratives et au plus un bloc `script`.

### 8.3 Templates

```
template Grunt(y: float, hp: int = 3) {
    Sprite:    { path: "bug.png" }
    Transform: { pos: (SPAWN_X, y) }
    Health:    hp
}
```

Un template est un modèle d'entité paramétré.

- Les paramètres sont typés et PEUVENT avoir une valeur par défaut. Les paramètres sans défaut viennent en premier.
- Les parenthèses sont optionnelles quand il n'y a aucun paramètre.
- Le corps suit les règles d'un corps d'entité. Les expressions peuvent utiliser les paramètres et les `const`.

### 8.4 Instanciation

```
Grunt(y: 300)
Grunt(300, 5)
Grunt(y: 300) as g1 { Health: 10 }
Boss() as boss
```

- Un `UIdent` suivi de `(` instancie un template. Les parenthèses sont **obligatoires** à l'appel, même sans argument.
- Les arguments peuvent être positionnels puis nommés, jamais l'inverse.
- `as lident` lie l'entité créée à une variable de type `entity`, visible dans le bloc courant et ses sous-blocs.
- Le corps optionnel **surcharge** les composants du template : un composant en champs nommés est fusionné champ par champ, une forme courte remplace le champ par défaut.
- Ordre d'application : composants du template, puis surcharge.

### 8.5 Boucle déclarative

```
for i in 0 to 4 {
    Grunt(y: 100 + i * 150)
}
```

Dans la couche déclarative, `for` est **déroulée à la compilation** : les bornes DOIVENT être des constantes. L'éditeur l'affiche comme un groupe paramétrique (section 14).

### 8.6 Évaluation statique

Dans la couche déclarative, une expression ne peut référencer que : les `const`, les paramètres de template, les variables de boucle, `index` (section 9.1) et les noms d'entités. Elle NE PEUT PAS référencer une variable `let` ou `mut` d'un script. Les références à un enfant (`boss::Turret`) sont valides dans la couche déclarative.

### 8.7 Relations parent-enfant

Une entité déclarée dans le corps d'une autre en est l'**enfant**.

```
Player {
    Transform: { pos: (100, 540) }
    Sprite:    { path: "ship.png" }

    Turret {                              // enfant nommé
        Transform: { pos: (20, 0) }       // relatif au parent
        Sprite:    { path: "turret.png" }
    }

    _ { Sprite: { path: "shield.png" } }  // enfant anonyme
}
```

- Le `Transform` d'un enfant est **local** : il est exprimé dans le repère de son parent.
- Détruire un parent détruit tous ses descendants (`despawn` est récursif).
- Un template peut déclarer des enfants ; ils sont créés avec chaque instance.
- À l'instanciation, un corps de surcharge peut cibler un enfant par son nom, la fusion suivant les règles de 8.4 :

```
Boss() as boss {
    Turret { Health: 10 }
}
```

- `boss::Turret` désigne l'enfant `Turret` de `boss` (type `entity`). `boss::Turret.Health.max` lit un champ de cet enfant.
- `parent` désigne le parent de `self`. Il n'est pas défini sur une entité racine.
- Un enfant n'a qu'un seul parent. Le reparentage à l'exécution n'est pas supporté en version 1.
- Les événements d'un enfant ne remontent pas au parent.

## 9. Niveaux et timeline

### 9.1 level

```
level "Stage 1" {
    seed: 42

    Background { ... }
    Player { ... }
    timeline { ... }
}
```

Un niveau contient des **propriétés** (`lident: valeur`), des entités initiales, au plus une `timeline` et au plus un bloc `script`. Le nom du niveau est une chaîne.

La seule propriété définie par cette RFC est :

| Propriété | Type  | Rôle                                         |
| --------- | ----- | -------------------------------------------- |
| `seed`    | `int` | graine des fonctions aléatoires (défaut : 0) |

Toute autre donnée (fond, défilement, musique, enchaînement des niveaux) appartient au jeu : elle s'exprime par des composants sur des entités, et le moteur réagit aux événements (`LevelComplete`) pour choisir la suite. Le registre PEUT déclarer des propriétés de niveau supplémentaires. Une propriété inconnue est une erreur.

Le registre PEUT en ajouter d'autres. Une propriété inconnue est une erreur.

### 9.2 timeline

La timeline regroupe des actions planifiées dans le temps. Elle est conçue pour être affichée comme une ligne de temps et modifiée graphiquement.
Une timeline est équivalente à un ensemble de tâches `script` (`at` → `await wait`, `when` → `await on`). Une implémentation PEUT la compiler ainsi ; la timeline existe pour l'éditeur, pas pour étendre le langage.

#### at

```
at 5s { ... }
at 5s every 500ms repeat 5 { ... }
```

- `at <duration>` exécute le bloc à cet instant. Le temps est mesuré en **temps de jeu** depuis le début du niveau.
- `every <duration> repeat <int>` répète le bloc `repeat` fois au total, la première fois à l'instant `at`. Dans le bloc, la variable implicite `index` (de 0 à `repeat - 1`) est disponible.
- La durée, l'intervalle et le nombre de répétitions DOIVENT être constants.
- Un `at` imbriqué dans un `when` est relatif à l'instant où le `when` s'est déclenché.

#### when

```
when boss.Death { emit LevelComplete }
when Player.Death count 3 { emit GameOver }
when boss.Hit once { ... }
```

`when <entité>.<Événement>` réagit à un événement émis par une entité.

- Sans modificateur, le bloc s'exécute à chaque occurrence.
- `once` : seulement à la première occurrence.
- `count N` : une seule fois, à la N-ième occurrence.

#### Actions autorisées

Le contenu d'un `at` ou d'un `when` est limité à des **actions**, pour rester éditable visuellement :

| Action | Exemple |
| --- | --- |
| instancier ou déclarer une entité | `Grunt(y: 100)` |
| affecter un composant | `boss.Health: 50` |
| émettre un événement global | `emit LevelComplete` |
| détruire une entité | `despawn boss` |
| `at` imbriqué, `for` déclaratif |  |

La logique conditionnelle ou temporelle libre est interdite ici : elle relève de `script`.

## 10. Couche script

Un bloc `script { ... }` peut apparaître dans un `level`, un `template` ou une entité. Il décrit un comportement qui s'exécute **dans le temps** et peut se suspendre.

```
Boss {
    Health: 100

    script {
        mut phase = 1
        loop {
            await on(Hit)
            if self.Health.max < 50 and phase == 1 {
                phase = 2
                Movement: sine(amplitude: 200, period: 1s)
            }
        }
    }
}
```

### 10.1 Sémantique d'exécution

- Chaque bloc `script` est une **tâche** indépendante, créée quand son entité (ou son niveau) est créé, détruite avec elle.
- Une tâche s'exécute jusqu'à rencontrer un `await`, puis se suspend. Elle reprend quand la condition est remplie.
- `self` désigne l'entité propriétaire et `parent` son parent (dans un script de niveau, ni l'un ni l'autre n'est défini).
- Les variables sont locales à la tâche et au bloc où elles sont déclarées.

### 10.2 Instructions

| Instruction | Forme | Remarque |
| --- | --- | --- |
| Constante | `let x = expr` | non modifiable |
| Variable | `mut x = expr` | modifiable |
| Affectation | `x = expr`, `self.Health.max = 5` | le type ne change pas |
| Composant | `Color: #ff0000`, `ref.Color: ...` | applique un composant, sur `self` si pas de préfixe |
| Condition | `if cond { } else if cond { } else { }` | `cond` est un `bool` |
| Boucle bornée | `for i in a``  to `` ``b { }` | bornes évaluées une fois à l'entrée |
| Boucle infinie | `loop { }` | voir 10.4 |
| Sortie | `break` | quitte la boucle la plus proche |
| Attente | `await ...` | voir 10.3 |
| Parallèle | `parallel { branch { } branch { } }` |  |
| Course | `race { branch { } branch { } }` |  |
| Actions | `emit E`, `despawn r`, instanciations | comme en 9.2 |

Dans un script, `for` PEUT avoir des bornes dynamiques. Comme dans la couche déclarative, `a``  t `` o ` `b` est semi-ouvert.

### 10.3 await

| Forme | Reprise de la tâche |
| --- | --- |
| `await wait(2s)` | après la durée donnée |
| `await on(Hit)` | quand `self` émet l'événement `Hit` |
| `await on(boss, Death)` | quand `boss` émet `Death` |
| `await on(Event)` dans un script de niveau | quand l'événement est émis globalement |

- `parallel` lance toutes ses branches et se termine quand **toutes** sont terminées.
- `race` se termine quand la **première** branche se termine et annule les autres.

### 10.4 Garanties de terminaison

- Un `loop` DOIT contenir au moins un `await` sur chaque chemin d'exécution possible, ou un `break`. Sinon : erreur de compilation. Cela interdit les boucles actives qui bloquent le serveur.
- Une implémentation DOIT imposer un budget d'instructions par tâche et par tick (recommandé : 10 000). Le dépasser est une erreur d'exécution, qui détruit la tâche sans arrêter le niveau.

## 11. Registre

Le langage ne connaît aucun composant. Les composants, événements, énumérations, constructeurs et propriétés sont fournis par un **registre** déclaré par le moteur. C'est ce qui permet le contrôle de types, l'autocomplétion et le choix des widgets dans l'éditeur.

Chaque entrée du registre DOIT fournir :

| Élément | Informations |
| --- | --- |
| Composant | nom, champs (nom, type, défaut, min/max éventuels), champ par défaut éventuel |
| Événement | nom, entités émettrices possibles |
| Énumération | nom, valeurs |
| Constructeur | nom `lident`, paramètres nommés typés, type retourné |
| Propriété de niveau | nom, type, défaut |
| Fonction | nom `lident`, paramètres typés, type retourné, catégorie (`pure` ou `effect`) |

*Non normatif* : un jeu de départ possible est `Transform`, `Sprite`, `Health`, `Velocity`, `Hitbox`, `Movement` ; les événements `Death`, `Hit`, `Offscreen`, `Collide`, `LevelComplete`, `GameOver`.

### Constructeurs

Un constructeur crée une valeur structurée à partir d'arguments nommés :

```
Movement: sine(amplitude: 50, period: 2s)
```

Il s'écrit en `snake_case` et se distingue d'un template par sa casse.

### 11.1 Fonctions natives

Le moteur enregistre des fonctions C++ appelables par `nom(args)`.

| Catégorie | Contraintes | Utilisable dans |
| --- | --- | --- |
| `pure`   | déterministe, aucun effet de bord | partout, y compris `const` si les arguments sont constants |
| `effect` | peut modifier le monde ou l'état du générateur aléatoire | actions (`at`, `when`) et `script` uniquement |

- Les types des arguments et du retour sont vérifiés au chargement.
- Une fonction NE DOIT PAS lire l'horloge réelle, un générateur global ou le système de fichiers. L'aléatoire passe par le générateur du contexte, initialisé avec `seed`.
- Une erreur renvoyée par la fonction est une erreur d'exécution (E5xx) : la tâche est détruite, le niveau continue.
- Une fonction ne peut pas se suspendre ; seul `await` suspend une tâche.

## 12. Bibliothèque standard

Ces fonctions sont enregistrées par défaut dans le registre (11.1). Le moteur PEUT en ajouter.

| Fonction | Signature | Catégorie |
| --- | --- | --- |
| `min`, `max` | `(a, b)` | `pure` |
| `abs` | `(x)` | `pure` |
| `clamp` | `(x, lo, hi)` | `pure` |
| `lerp` | `(a, b, t: float)` | `pure` |
| `sin`, `cos` | `(angle: float)`, en radians | `pure` |
| `rand_int` | `(lo: int, hi: int) -> int` | `effect` |
| `rand_float` | `(lo: float, hi: float) -> float` | `effect` |

Les fonctions aléatoires utilisent un générateur initialisé avec `seed` du niveau. Leur séquence d'appels est donc reproductible. Il n'y a pas d'autre source de non-déterminisme.

## 13. Modèle d'exécution

Le niveau avance par **ticks** à pas fixe, définis par le serveur. Une durée est convertie en ticks par `ceil(durée / pas)`.

À chaque tick, dans cet ordre :

1. le temps du niveau avance d'un pas ;
2. les `at` arrivés à échéance s'exécutent, dans l'ordre d'apparition dans le fichier ;
3. les `when` dont l'événement a été émis **au tick précédent** s'exécutent ;
4. les tâches `script` prêtes reprennent, dans leur ordre de création.

Les événements émis pendant un tick ne sont livrés qu'au tick suivant : il n'y a pas de réentrance.

**Déterminisme.** Le serveur est l'autorité : il exécute le niveau et crée les entités. Un même fichier avec la même graine et les mêmes événements d'entrée DOIT produire la même suite de créations d'entités. L'exécution NE DOIT dépendre ni de l'horloge réelle, ni de l'ordre de hachage, ni du système de fichiers.

## 14. Attributs, outils et round-trip

### 14.1 Attributs

Un attribut précède une entité, un `at`, un `when`, un template ou un niveau :

```
@id("e_3f2a")
@editor(collapsed: true)
_ { ... }
```

Syntaxe : `@lident` suivi d'arguments nommés optionnels entre parenthèses. L'exécution ignore les attributs. Réservés : `@id` (identité stable d'un élément) et `@editor` (métadonnées d'éditeur).

### 14.2 Garantie de round-trip

Un outil d'édition (éditeur visuel, formateur) qui lit puis réécrit un fichier DOIT respecter :

| Élément | Comportement |
| --- | --- |
| Couche déclarative | peut être normalisée (espaces, indentation) |
| Commentaires | conservés, rattachés au nœud qui les suit |
| Bloc `script` | conservé octet pour octet |
| Ordre des éléments | conservé |
| Champs omis | restent omis (pas de valeurs par défaut écrites) |
| Entité anonyme | reçoit un `@id` à la première sauvegarde |
| `for` et `every repeat` | restent des groupes ; une action « développer » les convertit en éléments explicites |

Cette garantie impose que le parseur produise un **arbre syntaxique concret** qui retient commentaires, espaces significatifs et positions.

### 14.3 Format canonique

Indentation de 4 espaces, fins de ligne `LF`, un composant par ligne, une ligne vide finale. Un formateur NE DOIT PAS réordonner les éléments.

## 15. Erreurs

Les erreurs se classent en trois catégories :

1. **Syntaxe** : jeton inattendu, accolade manquante.
2. **Sémantique statique**, détectée avant le lancement : nom inconnu, type incorrect, composant dupliqué, `loop` sans `await`, import cyclique, valeur non constante dans la couche déclarative.
3. **Exécution** : lecture d'un composant absent, division par zéro, budget d'instructions dépassé.

Format recommandé :

```
stage1.lvl:12:5: erreur[E201]: composant inconnu `Helth`
    Helth: 3
    ^^^^^
  aide : vouliez-vous dire `Health` ?
```

Une erreur de catégorie 3 DOIT être signalée au journal, détruire uniquement la tâche concernée et laisser le niveau continuer.

Codes réservés (*non normatif*) : `E1xx` syntaxe, `E2xx` noms et registre, `E3xx` types, `E4xx` vérifications structurelles, `E5xx` exécution.

## 16. Grammaire

Notation EBNF : `{ x }` répétition, `[ x ]` option, `|` alternative. `NL` est un saut de ligne, `UIDENT` et `LIDENT` sont définis en 5.4.

```
file        = { import } { topDecl } ;
import      = "import" STRING NL ;
topDecl     = constDecl | template | level ;

constDecl   = "const" IDENT "=" expr NL ;
template    = { attr } "template" UIDENT [ "(" [ params ] ")" ] entityBody ;
params      = param { "," param } ;
param       = LIDENT ":" type [ "=" expr ] ;
type        = "int" | "float" | "bool" | "string" | "vec2"
            | "color" | "duration" | "entity" | UIDENT ;

level       = { attr } "level" STRING "{" { levelItem } "}" ;
levelItem   = property | timeline | scriptBlock | entityStmt ;
property    = LIDENT ":" expr NL ;

entityBody  = "{" { bodyItem } "}" ;
bodyItem    = componentSet | entityStmt | forDecl | scriptBlock ;
entityStmt  = { attr } ( entityDecl | instance ) ;
entityDecl  = ( UIDENT | "_" ) entityBody ;
instance    = UIDENT "(" [ args ] ")" [ "as" LIDENT ] [ entityBody ] ;
args        = arg { "," arg } ;
arg         = [ LIDENT ":" ] expr ;
componentSet= [ ref "." ] UIDENT ":" compValue NL ;
compValue   = "{" field { "," field } "}" | expr ;
field       = LIDENT ":" expr ;
forDecl     = "for" LIDENT "in" expr "to" expr "{" { bodyItem } "}" ;

timeline    = "timeline" "{" { timelineItem } "}" ;
timelineItem= atBlock | whenBlock ;
atBlock     = { attr } "at" expr [ "every" expr "repeat" expr ] actionBlock ;
whenBlock   = { attr } "when" ref "." UIDENT [ "once" | "count" expr ] actionBlock ;
actionBlock = "{" { action } "}" ;
action      = entityStmt | componentSet | emit | despawn | atBlock | forAction ;
forAction   = "for" LIDENT "in" expr "to" expr actionBlock ;
emit        = "emit" UIDENT NL ;
despawn     = "despawn" ref NL ;

scriptBlock = "script" "{" { scriptStmt } "}" ;
scriptStmt  = letDecl | mutDecl | assign | componentSet | ifStmt | forStmt
            | loopStmt | "break" NL | awaitStmt | parallel | race
            | emit | despawn | entityStmt ;
letDecl     = "let" LIDENT "=" expr NL ;
mutDecl     = "mut" LIDENT "=" expr NL ;
assign      = ref { "." IDENT } "=" expr NL ;
ifStmt      = "if" expr block [ "else" ( ifStmt | block ) ] ;
forStmt     = "for" LIDENT "in" expr "to" expr block ;
loopStmt    = "loop" block ;
awaitStmt   = "await" ( "wait" "(" expr ")" | "on" "(" [ ref "," ] UIDENT ")" ) NL ;
parallel    = "parallel" "{" { branch } "}" ;
race        = "race" "{" { branch } "}" ;
branch      = "branch" block ;
block       = "{" { scriptStmt } "}" ;

attr        = "@" LIDENT [ "(" [ args ] ")" ] NL ;
ref         = IDENT | "self" | "parent" | ref "::" UIDENT ;

expr        = orExpr ;
orExpr      = andExpr { "or" andExpr } ;
andExpr     = notExpr { "and" notExpr } ;
notExpr     = "not" notExpr | cmpExpr ;
cmpExpr     = addExpr [ ( "==" | "!=" | "<" | ">" | "<=" | ">=" ) addExpr ] ;
addExpr     = mulExpr { ( "+" | "-" ) mulExpr } ;
mulExpr     = unary { ( "*" | "/" | "%" ) unary } ;
unary       = "-" unary | postfix ;
postfix     = primary { "." IDENT | "(" [ args ] ")" } ;
primary     = INT | FLOAT | BOOL | STRING | COLOR | DURATION | IDENT
            | "self" | "parent" | "(" expr [ "," expr ] ")" ;
```

## 17. Exemple complet

```
import "common/enemies.lvl"

const SCREEN_W = 1920
const SPAWN_X  = SCREEN_W + 64

template Boss {
    Sprite:    { path: "boss.png", layer: 3 }
    Transform: { pos: (SPAWN_X, 400) }
    Health:    100

    Turret {
        Transform: { pos: (-40, 0) }
        Sprite:    { path: "turret.png" }
    }

    script {
        mut phase = 1
        loop {
            await on(Hit)
            if self.Health.max < 50 and phase == 1 {
                phase = 2
                Movement: sine(amplitude: 200, period: 1s)
            }
        }
    }
}

level "Stage 1" {
    seed: 42

    Background {
        Sprite:    { path: "space.png" }
        Scrolling: { speed: 2.0 }
    }

    Player {
        Transform: { pos: (100, 540) }
        Sprite:    { path: "ship.png" }
        Health:    3
    }

    timeline {
        at 2s every 500ms repeat 5 {
            Grunt(y: 100 + 150 * index)
        }

        at 20s {
            Boss() as boss { Turret { Health: 10 } }
        }

        when boss.Death {
            at 2s { emit LevelComplete }
        }

        when Player.Death count 3 {
            emit GameOver
        }
    }
}
```

## 18. Relation avec la proposition initiale

Cette RFC reprend de la proposition de départ : les blocs `Nom { ... }`, la syntaxe `Composant: valeur`, `mut`, `loop`, `await on(...)`, `for` et l'entité anonyme `_`.

Elle s'en écarte sur trois points :

- **`if` réactif** (une condition qui applique des composants tant qu'elle est vraie) : exclu de la version 1. Il convient à une interface graphique, mais il est difficile à éditer visuellement dans un niveau. Le mot `if` n'est autorisé que dans un bloc `script`. Une extension future PEUT l'introduire sous un autre nom (voir section 19).
- **Les événements** viennent du jeu (`Death`, `Hit`…), pas de la souris.
- **Une timeline déclarative** est ajoutée, pour que l'éditeur dispose d'une ligne de temps.

## 19. Questions ouvertes

1. **Nom du langage** et extension de fichier (`LVL` et `.lvl` sont provisoires).
2. **Repère des coordonnées** : pixels, origine en haut à gauche, ou unités de monde indépendantes de la résolution ?
3. **Un niveau par fichier** ou plusieurs ? Cette RFC autorise plusieurs `level` par fichier ; la section 7.2 interdit seulement de les importer. 
4. **Pas de simulation** : valeur du tick du serveur, qui détermine la précision des durées. 
5. **`if` réactif** : faut-il une construction dédiée (`bind`, `while`…) pour une version 2 ? 
6. **Contenu du registre** : liste exacte des composants et événements, à figer avec la personne qui maintient siecs. 
7. **Tags et requêtes** : faut-il pouvoir écrire `when any(Enemy).Death` ? 
8. **Gestion des erreurs d'exécution** : journal seul, ou événement `ScriptError` observable ? 
9. **Accès aux enfants** : la syntaxe `::` (`boss::Turret`) est-elle la bonne, ou faut-il une autre forme ? 
10. **Fonctions `effect`** : peuvent-elles créer des entités, ou seulement modifier des composants ? 
11. **Reparentage** à l'exécution : faut-il le supporter en version 2 ?

## 20. Historique

| Version | Date | Changement |
| --- | --- | --- |
| 0.1 | 2026-10-20 | Premier brouillon |
| 0.2 | 2026-10-27 | Suppression de `version`, des propriétés métier du niveau ; ajout des relations parent-enfant (8.7) et des fonctions natives (11.1) |