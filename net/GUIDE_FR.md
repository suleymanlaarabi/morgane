# SInet : utiliser le réseau avec SIECS

Ce guide suppose que tu connais déjà SIECS. Il explique l'API réseau actuelle, son fonctionnement et ses coûts. Les exemples utilisent C++23 et Bake.

## 1. À quoi sert `.reliable()` ?

Le transport utilise UDP. Un datagramme peut être perdu, dupliqué ou arriver après un autre.

```cpp
net::replicate<Position>();             // Pas de retransmission.
net::replicate<Health>().reliable();    // Retransmission jusqu'à ACK.
```

Le mode par défaut s'appelle `unreliable_sequenced`. Tu peux aussi l'écrire explicitement avec `.unreliable_sequenced()`. Pour un composant, la réception suit une révision et ignore une valeur plus ancienne que celle déjà appliquée : une vieille position arrivée en retard ne remplace pas la nouvelle.

Un ACK est un acquittement : le destinataire indique qu'il a reçu le datagramme.

Sans `.reliable()` :

1. Le serveur fait `player.set(Position{10, 5})`.
2. Le réseau envoie la valeur au prochain envoi réseau.
3. Si le datagramme est perdu, cette mise à jour est perdue.
4. Le prochain `.set()` pourra envoyer une nouvelle position.

Il n'y a **aucun renvoi automatique de la dernière valeur** pour ce mode. Si tu ne fais plus de `.set()`, le client peut conserver une ancienne valeur.

Avec `.reliable()` :

1. Le serveur fait `player.set(Health{80})`.
2. Le datagramme envoyé est conservé dans une fenêtre de retransmission.
3. Sans ACK après le délai prévu, il est renvoyé.
4. Après ACK, sa place est libérée.

La réception fiable remet les datagrammes fiables en ordre et évite leur application répétée. Cette garantie vaut pendant une connexion fonctionnelle : une déconnexion interrompt la livraison.

### Le coût

Le mode fiable utilise de la mémoire pour les datagrammes en attente, du suivi d'ACK et de la bande passante pour les retransmissions. Une perte peut retarder les datagrammes fiables suivants : ils attendent celui qui manque.

Actuellement, chaque connexion possède une seule séquence fiable, avec une fenêtre de 64 datagrammes en vol. Les messages fiables partagent donc cet ordre ; il n'y a pas d'API de canaux fiables indépendants.

| Donnée | Choix habituel | Pourquoi |
| --- | --- | --- |
| Position régulièrement actualisée | Défaut | La prochaine valeur remplace l'ancienne |
| Input de déplacement régulièrement actualisé | Défaut | La commande récente est la plus utile |
| Vie modifiée occasionnellement | `.reliable()` | Une perte pourrait laisser une valeur incorrecte longtemps |
| Action ponctuelle importante | Event fiable | Il faut recevoir l'action |
| Effet visuel facultatif | Event sans fiabilité | Une perte est acceptable |

**Un composant reste un état, même en mode fiable.** Plusieurs `.set()` sur le même composant peuvent être regroupés avant l'envoi : seule la dernière valeur en attente est conservée. Pour recevoir chaque occurrence d'une action, utilise un event.

## 2. Premier exemple : serveur et client

Les types réseau doivent être partagés. Le client et le serveur déclarent le même schéma, avec les mêmes options.

### `shared.h`

```cpp
#pragma once
#include <sinet/net.h>

struct Position {
    reflected(float x; float y;)
};

struct Health {
    reflected(int value;)
};

inline void register_network() {
    net::replicate<Position>();
    net::replicate<Health>().reliable();
}
```

### `server.cpp`

```cpp
#include <sinet/server.h>
#include "shared.h"

int main() {
    ecs::init({ .worker_threads = ECS_WORKERS_AUTO });

    ecs::import<net::Server>(net::ServerConfig{
        .port = 4242,
        .tick_rate = 30,
        .max_clients = 128,
    });

    register_network();

    ecs::observe<net::OnConnect>()
        .each([](ecs::entity connection) {
            ecs::entity::create()
                .set(Position{0.f, 0.f})
                .set(Health{100})
                .add<net::Replicated>()
                .relate<net::OwnedBy>(connection);
        });

    ecs::run();
}
```

### `client.cpp`

```cpp
#include <sinet/client.h>
#include <iostream>
#include "shared.h"

int main() {
    ecs::init();

    ecs::import<net::Client>(net::ClientConfig{
        .address = "127.0.0.1",
        .port = 4242,
    });

    register_network();

    ecs::observe<ecs::OnSet>()
        .each([](ecs::entity, const Position &position) {
            std::cout << position.x << ", " << position.y << '\n';
        });

    ecs::run();
}
```

Lance le serveur, puis le client. La connexion déclenche `OnConnect` sur le serveur. Le serveur crée le joueur ; le client reçoit automatiquement une entité avec ses composants.

L'affichage console sert uniquement à constater la réception. Évite de l'utiliser pour chaque position dans un jeu réel.

**Ordre d'initialisation :** initialise SIECS, importe le module réseau, déclare tous les types réseau, puis crée les entités répliquées et lance la boucle. Le registre se fige à sa première utilisation par le runtime, qui peut avoir lieu dès l'ajout de `Replicated`. Termine donc toutes les déclarations avant cet ajout.

## 3. Compilation avec Bake

Le package est défini dans [project.json](project.json), avec l'identifiant `sinet`. Ses dépendances directes sont `siecs` et `sireflect` ; la configuration propage également la dépendance à pthread.

Depuis la racine du projet :

```sh
bake build net
bake build server
bake build client
```

Le client actuel du projet utilise raylib : adapte les chemins raylib dans [client/project.json](../client/project.json). Le petit exemple console ci-dessus n'a pas besoin de raylib.

Pour tes applications Bake, déclare notamment :

```json
"value": {
  "language": "cpp",
  "use": ["siecs", "sinet"],
  "sources": ["src"]
},
"lang.cpp": {
  "cpp-standard": "c++23"
}
```

C'est un extrait à intégrer à ton `project.json`, avec l'identifiant et le type de ton application. Les inclusions publiques sont `<sinet/server.h>`, `<sinet/client.h>` et `<sinet/net.h>`.

## 4. Répliquer une entité et ses changements

La déclaration d'un type et la sélection d'une entité sont deux choses distinctes :

```cpp
net::replicate<Position>(); // Ce type peut circuler serveur → clients.

auto enemy = ecs::entity::create()
    .set(Position{20.f, 3.f})
    .add<net::Replicated>(); // Cette entité participe à la réplication.
```

Une entité sans `Replicated` reste locale. Un composant non déclaré au registre réseau reste local, même sur une entité répliquée.

Sur le serveur :

```cpp
enemy.set(Position{25.f, 3.f}); // Mise à jour.
enemy.remove<Position>();     // Suppression du composant.
enemy.kill();                 // Destruction distante de l'entité.
```

Enlever `Replicated` retire également l'entité des clients, tout en conservant l'entité sur le serveur.

Le spawn, le despawn, les relations réseau et les suppressions de composants sont fiables. L'état initial envoyé lors d'un spawn est lui aussi fiable, même si les mises à jour ordinaires de `Position` ne le sont pas. Un client qui devient intéressé par une entité reçoit son état conservé par le serveur.

### `.set()` est le contrat de modification

```cpp
auto next = player.get<Position>();
next.x += 1.f;
player.set(next);
```

N'utilise pas de référence mutable pour modifier directement un composant répliqué. Le framework observe les `.set()` ; il ne recherche pas des modifications cachées en mémoire. En Debug, l'enregistrement demande la politique SIECS `EcsSetOnly` pour ces composants. En Release, le respect de ce contrat reste nécessaire.

Un `.set()` de la même valeur déclenche aussi du travail : aucune comparaison automatique n'est effectuée pour l'éviter. C'est ton gameplay qui décide quand une nouvelle valeur mérite d'être publiée.

Pour les tags vides déclarés avec `replicate<T>()`, l'ajout passe par `.add<T>()` et est observé via `OnAdd`.

## 5. Identifier les connexions et le joueur local

Une connexion est une entité ECS avec les composants réseau :

| Composant | Rôle |
| --- | --- |
| `Connection` | Identifie une entité de connexion |
| `PeerId` | Identifiant du pair |
| `Connected` | Connexion établie |
| `Latency` | Estimation du RTT en millisecondes, donc un aller-retour |
| `Stats` | Compteurs d'envoi, réception et retransmission |

Sur le client, retiens l'entité donnée à `OnConnect` :

```cpp
struct LocalConnection { ecs::entity entity; };

// Après l'import du module, avant ecs::run().
ecs::set_resource(LocalConnection{});

ecs::observe<net::OnConnect>()
    .each([](ecs::entity connection) {
        ecs::resource<LocalConnection>().entity = connection;
    });
```

Le joueur local est celui dont `OwnedBy` cible cette connexion :

```cpp
bool mine = player.target<net::OwnedBy>().id()
         == ecs::resource<LocalConnection>().entity.id();
```

Ne compare pas les identifiants ECS du serveur et du client : ils sont locaux à chaque world. Le framework associe les entités avec `NetworkId`, un identifiant réseau compact de 32 bits comprenant un index et une génération. Il est ajouté automatiquement ; ne le modifie pas.

`OwnedBy` donne le droit d'envoyer les inputs du joueur. Il ne rend pas le joueur invisible aux autres clients : la visibilité dépend des relations de scope décrites plus loin.

## 6. Envoyer les inputs client → serveur

Ajoute à `shared.h` :

```cpp
struct PlayerInput {
    reflected(float move_x; float move_y;)
};
```

Et dans `register_network()`, des deux côtés :

```cpp
net::input<PlayerInput>().owner_only();
```

Le client publie son input sur son joueur :

```cpp
player.set(PlayerInput{1.f, 0.f});
```

Le serveur reçoit ce composant sur l'entité correspondante. Ton système de déplacement lit `const PlayerInput &`, calcule la nouvelle position à partir de la vitesse et de ton pas de simulation, puis appelle `.set(Position{...})`.

Le client envoie les commandes ; le serveur décide du résultat. Un client ne peut pas publier `Position` ou `Health` sur le serveur via cette API.

### Exemple de système client

Avec la resource `LocalConnection` de l'étape précédente :

```cpp
ecs::system("PublishInput")
    .each([](ecs::entity player,
             const net::Replicated &,
             const Position &,
             ecs::res<LocalConnection> local) {
        if (player.target<net::OwnedBy>().id() != local->entity.id())
            return;

        // Remplacer par la lecture clavier/manette.
        player.set(PlayerInput{1.f, 0.f});
    });
```

Ici, le jeu republie volontairement l'état de commande chaque frame. Le réseau regroupe les valeurs en attente et transmet au rythme réseau. Ce choix permet de récupérer après la perte d'un input ; le framework lui-même ne renvoie pas automatiquement les inputs non fiables.

Pense aussi à publier la valeur neutre à l'arrêt du mouvement. Si tu ne l'envoies qu'une fois en UDP non fiable et qu'elle se perd, le serveur peut conserver la commande précédente. Pour un input continu, publier régulièrement l'état courant, y compris neutre, évite de dépendre d'un unique paquet.

Dans l'implémentation actuelle, l'ownership est obligatoire pour les inputs sur une entité et pour les events client ciblant une entité répliquée. `.owner_only()` exprime ce contrat ; il n'existe pas de mode permettant d'écrire sur les entités des autres joueurs.

Cette vérification réseau ne remplace pas tes règles de gameplay : vitesse maximale, cooldown, ressources disponibles, etc.

## 7. Envoyer une action ponctuelle avec un event

Pour une action qui doit être reçue une fois par déclenchement :

```cpp
struct Shoot {};

// Dans register_network(), des deux côtés.
net::event<Shoot>().client_to_server().reliable();
```

Côté client :

```cpp
ecs::trigger<Shoot>(player);
```

Côté serveur :

```cpp
ecs::observe<Shoot>()
    .each([](ecs::entity player) {
        // Vérifier le cooldown et créer le projectile.
    });
```

Les events peuvent avoir des données réfléchies, comme les composants. Pour un event avec payload, passe la donnée au `trigger` selon l'API SIECS :

```cpp
struct JoinLobby { reflected(uint32_t lobby_id;) };

net::event<JoinLobby>().client_to_server().reliable();

JoinLobby request{7};
ecs::trigger<JoinLobby>(connection, &request);
```

Un event client ciblant sa connexion arrive sur la connexion correspondante du serveur. Cela convient aux actions qui précèdent la création du joueur, comme une demande de lobby. Le serveur résout ici `lobby_id` avec son propre registre de lobbies ; ce nombre n'est pas un `ecs_entity_t` distant.

### Serveur → client

```cpp
struct Accepted {};
net::event<Accepted>().server_to_clients().reliable();

// Sur le serveur : uniquement ce client.
ecs::trigger<Accepted>(connection);

// Sur le serveur : tous les clients qui voient ce joueur.
ecs::trigger<Accepted>(player);
```

Le client reçoit ces events avec un observer ECS normal.

`net::event<T>()` choisit client → serveur par défaut ; `.client_to_server()` le rend explicite. Utilise des types d'events distincts pour les deux directions : un même type ne peut être enregistré qu'une fois dans un registre.

Les events sans `.reliable()` peuvent être perdus. Ils n'ont pas le suivi de révision d'un composant : ne suppose pas que deux actions non fiables seront exécutées dans leur ordre d'émission.

Un event ECS déclenché localement exécute aussi les observers locaux habituels. Place donc les observers de gameplay autoritaire sur le serveur.

## 8. Lobbies, parties et visibilité

Les relations sont enregistrées par le module : tu peux directement les utiliser.

Sur le serveur :

```cpp
auto game = ecs::entity::create();
auto lobby = ecs::entity::create().relate<net::InInstance>(game);

connection.relate<net::MemberOf>(lobby);

auto player = ecs::entity::create()
    .set(Position{0.f, 0.f})
    .add<net::Replicated>()
    .relate<net::OwnedBy>(connection)
    .relate<net::InInstance>(game);
```

Le scope de partie d'une connexion vient de sa relation `InInstance`, si elle existe, sinon de la relation `InInstance` de son lobby.

Pour qu'une entité soit visible :

- Si elle a `MemberOf`, elle doit appartenir au même lobby que la connexion.
- Si elle a `InInstance`, elle doit appartenir à la même partie que la connexion.
- Si elle a les deux, les deux conditions doivent être remplies.
- Si elle n'a aucune des deux, elle est visible par toutes les connexions établies.

Pour changer de lobby :

```cpp
connection.relate<net::MemberOf>(otherLobby);
```

Le réseau recalcule les intérêts concernés. Les entités devenues invisibles sont supprimées sur ce client ; les nouvelles entités visibles sont envoyées avec leur état initial.

Cela ne déplace pas automatiquement le joueur vers une autre partie. Change aussi `player.relate<net::InInstance>(otherGame)` si ton gameplay le demande.

La visibilité est conservée par connexion. Les changements de membership peuvent parcourir le registre réseau des entités pour recalculer les intérêts ; ce parcours n'est pas effectué pour détecter des modifications de composants à chaque tick.

Les lobbies et parties peuvent rester de simples entités de scope. Pour transmettre aussi leurs composants, déclare ces composants et ajoute `Replicated` sur ces entités.

## 9. Schémas, IDs et codecs

Tu ne choisis pas d'ID de type. Le nom du type donne un hash stable de 64 bits ; les types sont triés pour établir leurs slots réseau de 16 bits. Le handshake compare les IDs et les signatures de schéma.

L'ordre de déclaration peut différer entre serveur et client. En revanche, la liste complète des types, les noms, les codecs, les directions et les options de fiabilité doivent correspondre. Ce handshake ne négocie pas un sous-ensemble commun entre deux versions différentes.

Pour donner un nom explicite :

```cpp
net::replicate<Position>().name("game.Position");
```

Le codec automatique transmet les champs réfléchis, sans padding C++, en little endian. Le plan des champs est préparé au démarrage ; il n'y a pas de recherche dynamique des métadonnées de reflection pour chaque packet. L'encodage parcourt ensuite les champs préparés.

Les types réseau doivent être trivialement copiables et avoir une disposition standard. Le codec automatique prend en charge les scalaires compatibles, les structures réfléchies imbriquées et les tableaux fixes. N'y mets pas de pointeurs, de `std::string` ou de conteneurs dynamiques.

### Codec personnalisé

```cpp
net::replicate<Position>().codec<QuantizedPosition>();
```

Le codec doit exposer :

```cpp
struct QuantizedPosition {
    static constexpr std::string_view schema_name = "game.Position.cm.v1";
    static constexpr std::size_t wire_size = 4;

    static void encode(const Position &, std::span<std::byte>);
    static bool decode(Position &, std::span<const std::byte>);
};
```

Ce fragment donne l'interface : les fonctions sont à implémenter. Par exemple, deux coordonnées en centimètres encodées sur deux entiers signés de 16 bits occupent 4 octets au lieu de 8 pour deux floats. Cela impose une précision et une plage de coordonnées adaptées à ton jeu. Le codec doit écrire un format explicite et stable, sans allocation ; `decode` renvoie `false` pour une donnée invalide.

Pour un type sans champs réfléchis, fournis un codec et un nom explicite si nécessaire. `.name()` ne remplace pas un codec.

## 10. Régler le rythme réseau et les capacités

```cpp
ecs::import<net::Server>(net::ServerConfig{
    .port = 4242,
    .tick_rate = 30,
    .max_clients = 128,
    .bind_address = "0.0.0.0",
    .limits = {
        .max_entities = 4096,
        .bytes_per_tick = 64 * 1024,
        .timeout_seconds = 10,
    },
});
```

| Paramètre | Effet |
| --- | --- |
| `ServerConfig::tick_rate` | Fréquence maximale des envois serveur, indépendante du rythme gameplay |
| `max_clients` | Nombre maximal de connexions serveur |
| `max_entities` | Capacité du registre des entités répliquées ; les identités de scope ont leur propre réserve de même capacité |
| `bytes_per_tick` | Budget d'envoi par connexion et par tick, consommé par les données et retransmissions ; les heartbeats sont envoyés hors de ce budget |
| `timeout_seconds` | Délai sans réception avant déconnexion |

Sur le client, le rythme de publication vient de `ClientConfig::limits.tick_rate`, par défaut 30. Sur le serveur, le `tick_rate` principal remplace celui de `limits`.

Un tick réseau à 30 Hz ne fait pas passer le gameplay à 30 Hz. L'envoi est planifié dans la boucle ECS ; il ne peut toutefois pas s'exécuter plus souvent que cette boucle.

Le client doit avoir une capacité `max_entities` au moins égale à celle annoncée par le serveur.

Les limites actuelles incluent 64 types réseau, 256 octets encodés par composant/event et des datagrammes de 1200 octets. Les messages sont regroupés dans les datagrammes ; les gros messages ne sont pas fragmentés par une API publique.

Les buffers sont dimensionnés à l'initialisation et à l'admission des connexions. Les outboxes sont réservées par connexion, avec une taille liée à `max_entities` et au nombre de types. Augmenter fortement ces capacités augmente donc la mémoire, même si peu d'entités sont actives. Le budget d'envoi est une limite de débit, pas une garantie que toute la file sera vidée dans le tick.

## 11. Déconnexion et diagnostic

```cpp
ecs::observe<net::OnDisconnect>()
    .each([](ecs::observer_event event) {
        auto reason = *event.trigger_data<net::DisconnectReason>();
        // Réagir selon reason.
    });
```

Les raisons exposées sont `remote`, `timeout`, `schema_mismatch`, `capacity` et `congestion`.

La connexion est retirée après le déclenchement de cet event. La suppression des joueurs, leur conservation pour une reconnexion ou leur remplacement par un bot restent des décisions de gameplay : le réseau ne détruit pas automatiquement tous les joueurs possédés.

Tu peux lire `connection.get<net::Stats>()` et `connection.get<net::Latency>()`. `retransmissions` permet notamment de constater le coût de la fiabilité.

Si le client ne reçoit rien, vérifie dans cet ordre : adresse IPv4 et port, connexion établie, schémas identiques, tag `Replicated`, puis relations de visibilité. Si un changement n'arrive pas, vérifie qu'il est publié avec `.set()` et considère une éventuelle perte en mode non fiable.

## 12. Ce qui se passe en interne et ce qui reste à faire

Le runtime suit ce chemin :

```text
Thread IO → file de réception → NetReceive → gameplay ECS
           observers → file de changements → NetCollectChanges
           → NetSend au rythme réseau → file d'envoi → thread IO
```

La réception est placée avant `EcsPreUpdate`, la collecte et l'envoi après `EcsPostUpdate`. Le thread socket ne touche jamais le world.

Les observers encodent les modifications et les placent dans une file bornée. Cette file de changements est protégée par un mutex pour accepter les publications des workers ECS. Les échanges avec le thread IO utilisent des files préallouées. Les IDs réseau permettent un accès direct au registre d'entités.

Les codecs et fonctions d'application sont appelés via des pointeurs de fonctions stockés dans les descripteurs ; ce chemin n'utilise pas de dispatch virtuel.

Il n'y a ni dirty mask, ni comparaison périodique de composants, ni suivi automatique des écritures par référence. En contrepartie, chaque `.set()` réseau a un coût d'observer, d'encodage et de mise en file. La diffusion ajoute du travail par connexion intéressée. Le chemin de transport des datagrammes n'alloue pas un objet heap par packet ; la création d'entités ECS et l'admission de connexions peuvent allouer.

L'état encodé courant est conservé pour les nouveaux clients et les changements d'intérêt. Les mises à jour transmettent le composant encodé complet : **pas de delta snapshot par rapport au dernier ACK**.

Le transport actuel utilise des sockets UDP Linux et des adresses IPv4 numériques. Il n'inclut pas de résolution DNS, de traversal NAT, de chiffrement ou d'authentification applicative.

L'interpolation, la prediction client et la reconciliation ne sont pas implémentées. Le client reçoit l'état autoritaire ; ajouter un affichage interpolé ou une simulation prédite demanderait une couche supplémentaire et des choix explicites de gameplay et de performance.

## 13. Où retrouver l'implémentation

| Fichier | Contenu |
| --- | --- |
| [include/sinet/net.h](include/sinet/net.h) | Composants, relations et builders de déclaration |
| [include/sinet/server.h](include/sinet/server.h) / [client.h](include/sinet/client.h) | Configurations publiques et modules |
| [src/registry.cpp](src/registry.cpp) | Schémas, slots et codecs automatiques |
| [src/replication.cpp](src/replication.cpp) | Observers, état conservé et intérêts |
| [src/receive.cpp](src/receive.cpp) | Réception, application ECS et installation du pipeline |
| [src/send.cpp](src/send.cpp) | Handshake, batching, budgets et retransmissions |
| [src/protocol.cpp](src/protocol.cpp) | En-têtes, séquences et ACK |
| [src/transport.cpp](src/transport.cpp) | Sockets, thread IO et files de datagrammes |
| [src/runtime.cpp](src/runtime.cpp) | Initialisation, outboxes et déconnexion |
| [server/net](../server/net) / [client/net](../client/net) | Implémentation de l'import des modules |
| [tests](../tests) | Exemples et vérifications du protocole et des scénarios réseau |

Le changement SIECS associé partage le cache des IDs d'events C++ entre unités de compilation, dans `siecs/cpp/observer.hpp`. Il évite que le même event obtienne des IDs différents selon le fichier qui l'utilise ; il n'ajoute pas de recherche dans le chemin d'émission d'un event.
