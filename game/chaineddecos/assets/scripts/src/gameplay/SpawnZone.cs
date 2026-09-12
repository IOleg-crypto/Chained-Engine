using System;
using Chained;

namespace ChainedDecos.Scripts
{
    public class SpawnZone : Script
    {
        private bool IsPlayerInside = false;

    public override void OnUpdate(float deltaTime)
    {
        // Only the host runs respawn logic — clients rely on authoritative state
        if (Network.IsClient)
            return;

        // Find the local player entity (by ownership in network, or first PlayerComponent)
        ulong playerId = 0;
        if (Network.IsConnected)
        {
            ulong[] owned = Entity.FindAllWithComponent<NetworkIdentityComponent>();
            foreach (ulong id in owned)
            {
                Entity e = new Entity(id);
                var netId = e.GetComponent<NetworkIdentityComponent>();
                if (netId != null && netId.IsOwner)
                {
                    playerId = id;
                    break;
                }
            }
        }

        if (playerId == 0)
        {
            ulong[] players = Entity.FindAllWithComponent<PlayerComponent>();
            if (players.Length == 0)
                return;
            playerId = players[0];
        }

        Entity player = new Entity(playerId);
        TransformComponent? playerTransform = player.GetComponent<TransformComponent>();
        if (playerTransform == null)
            return;

            ulong[] spawnEntities = Entity.FindAllWithComponent<SpawnComponent>();
            if (spawnEntities.Length == 0)
                return;

            TransformComponent? spawnTransform = Entity.GetComponent<TransformComponent>();
            SpawnComponent? thisSpawnComp = Entity.GetComponent<SpawnComponent>();
            if (spawnTransform == null || thisSpawnComp == null || !thisSpawnComp.IsActive)
                return;

            Vector3 zoneSize = thisSpawnComp.ZoneSize;
            float halfX = Math.Abs(zoneSize.X) * 0.5f;
            float halfY = Math.Abs(zoneSize.Y) * 0.5f;
            float halfZ = Math.Abs(zoneSize.Z) * 0.5f;

            if (halfX < 0.5f) halfX = 1.0f;
            if (halfY < 0.5f) halfY = 1.0f;
            if (halfZ < 0.5f) halfZ = 1.0f;

            float dx = Math.Abs(playerTransform.Translation.X - spawnTransform.Translation.X);
            float dy = Math.Abs(playerTransform.Translation.Y - spawnTransform.Translation.Y);
            float dz = Math.Abs(playerTransform.Translation.Z - spawnTransform.Translation.Z);

            IsPlayerInside = (dx <= halfX && dy <= halfY && dz <= halfZ);

            if (IsPlayerInside)
            {
                foreach (ulong id in spawnEntities)
                {
                    Entity spawner = new Entity(id);
                    SpawnComponent? sc = spawner.GetComponent<SpawnComponent>();
                    if (sc != null && sc.IsActive)
                    {
                        sc.IsCheckpoint = (id == Entity.ID);
                    }
                }
            }

            // Respawn player if fallen into the void (below Y = -35) or manually with F
            if (Input.IsKeyPressed(Key.F))
            {
                Respawn(player, spawnEntities);
            }
        }

        private void Respawn(Entity player, ulong[] spawnEntities)
        {
            foreach (ulong id in spawnEntities)
            {
                Entity spawner = new Entity(id);
                SpawnComponent? spawnComp = spawner.GetComponent<SpawnComponent>();
                if (spawnComp != null && spawnComp.IsActive && spawnComp.IsCheckpoint)
                {
                    TeleportTo(player, spawner, spawnComp);
                    return;
                }
            }

            // Fallback to first active spawn zone if no checkpoint is active
            foreach (ulong id in spawnEntities)
            {
                Entity spawner = new Entity(id);
                SpawnComponent? spawnComp = spawner.GetComponent<SpawnComponent>();
                if (spawnComp != null && spawnComp.IsActive)
                {
                    TeleportTo(player, spawner, spawnComp);
                    return;
                }
            }
        }

        private void TeleportTo(Entity player, Entity spawnEntity, SpawnComponent spawnComp)
        {
            TransformComponent? spawnTransform = spawnEntity.GetComponent<TransformComponent>();
            TransformComponent? playerTransform = player.GetComponent<TransformComponent>();
            RigidBodyComponent? playerRb = player.GetComponent<RigidBodyComponent>();

            if (spawnTransform == null || playerTransform == null)
                return;

            Vector3 targetPos = spawnTransform.Translation + spawnComp.SpawnPoint;
            if (Math.Abs(spawnComp.SpawnPoint.Y) < 0.01f)
            {
                targetPos.Y += 0.5f;
            }

            playerTransform.Translation = targetPos;

            if (playerRb != null)
            {
                playerRb.ForceSetVelocity(Vector3.Zero);
                playerRb.Velocity = Vector3.Zero;
            }

            Log.Info("spawnzone: Player respawned at checkpoint.");
        }
    }
}