// ============================================================================
// MagnaundasoniGeometry.cs – Acoustic geometry component
// ============================================================================
using System;
using System.Collections.Generic;
using UnityEngine;

namespace Magnaundasoni
{
    [AddComponentMenu("Magnaundasoni/Acoustic Geometry")]
    [RequireComponent(typeof(MeshFilter))]
    public class MagnaundasoniGeometry : MonoBehaviour
    {
        // ----- Inspector ---------------------------------------------------
        [Header("Acoustic Geometry")]
        [Tooltip("How the engine should treat this geometry for updates.")]
        [SerializeField] private MagDynamicFlag _dynamicFlag = MagDynamicFlag.Static;

        [Tooltip("Acoustic material to assign to this geometry.")]
        [SerializeField] private MagnaundasoniMaterial _acousticMaterial;

        // ----- Public Properties -------------------------------------------
        public uint NativeGeometryID => _geometryID;
        public bool IsRegistered => _registered;
        public MagDynamicFlag DynamicFlag => _dynamicFlag;

        // ----- Private State -----------------------------------------------
        private uint _geometryID;
        private bool _registered;
        private MeshFilter _meshFilter;
        private Matrix4x4 _lastTransformMatrix;

        // ----- Lifecycle ---------------------------------------------------
        private void OnEnable()
        {
            _meshFilter = GetComponent<MeshFilter>();
            RegisterGeometry();
        }

        private void OnDisable()
        {
            UnregisterGeometry();
        }

        private void Update()
        {
            if (!_registered) return;
            if (_dynamicFlag == MagDynamicFlag.Static) return;

            Matrix4x4 currentMatrix = transform.localToWorldMatrix;
            if (currentMatrix != _lastTransformMatrix)
            {
                UpdateTransform(currentMatrix);
                _lastTransformMatrix = currentMatrix;
            }
        }

        public void SetDynamicFlag(MagDynamicFlag flag)
        {
            bool needsReregister = _registered && flag != _dynamicFlag;
            _dynamicFlag = flag;
            if (needsReregister)
            {
                UnregisterGeometry();
                RegisterGeometry();
            }
        }

        public void SetMaterial(MagnaundasoniMaterial material)
        {
            _acousticMaterial = material;
            if (_registered)
            {
                UnregisterGeometry();
                RegisterGeometry();
            }
        }

        // ----- Registration ------------------------------------------------
        private unsafe void RegisterGeometry()
        {
            var engine = MagnaundasoniEngine.Instance;
            if (!engine.IsInitialized || _meshFilter == null) return;

            Mesh mesh = _meshFilter.sharedMesh;
            if (mesh == null) return;

            uint materialID = 0;
            if (_acousticMaterial != null && _acousticMaterial.IsRegistered)
                materialID = _acousticMaterial.NativeMaterialID;

            Vector3[] meshVertices = mesh.vertices;
            int[] meshTriangles = mesh.triangles;

            // Transform vertices to world space
            float[] worldVertices = new float[meshVertices.Length * 3];
            Matrix4x4 ltw = transform.localToWorldMatrix;
            for (int i = 0; i < meshVertices.Length; i++)
            {
                Vector3 wp = ltw.MultiplyPoint3x4(meshVertices[i]);
                worldVertices[i * 3 + 0] = wp.x;
                worldVertices[i * 3 + 1] = wp.y;
                worldVertices[i * 3 + 2] = wp.z;
            }

            uint[] indices = new uint[meshTriangles.Length];
            for (int i = 0; i < meshTriangles.Length; i++)
                indices[i] = (uint)meshTriangles[i];

            fixed (float* vertPtr = worldVertices)
            fixed (uint* idxPtr = indices)
            {
                var desc = new MagGeometryDesc
                {
                    vertices    = vertPtr,
                    vertexCount = (uint)meshVertices.Length,
                    indices     = idxPtr,
                    indexCount  = (uint)indices.Length,
                    materialID  = materialID,
                    dynamicFlag = (uint)_dynamicFlag
                };

                try
                {
                    _geometryID = MagAPI.GeometryRegister(engine.NativeHandle, desc);
                    _registered = true;
                    _lastTransformMatrix = ltw;
                }
                catch (MagnaundasoniException ex)
                {
                    Debug.LogError($"[Magnaundasoni] Geometry register failed: {ex.Message}");
                }
            }
        }

        private void UnregisterGeometry()
        {
            if (!_registered) return;

            var engine = MagnaundasoniEngine.Instance;
            if (engine != null && engine.IsInitialized)
            {
                try { MagAPI.GeometryUnregister(engine.NativeHandle, _geometryID); }
                catch (MagnaundasoniException) { }
            }
            _registered = false;
        }

        private void UpdateTransform(Matrix4x4 matrix)
        {
            var engine = MagnaundasoniEngine.Instance;
            if (!engine.IsInitialized) return;

            float[] m = new float[16];
            for (int col = 0; col < 4; col++)
                for (int row = 0; row < 4; row++)
                    m[col * 4 + row] = matrix[row, col]; // column-major

            try { MagAPI.GeometryUpdateTransform(engine.NativeHandle, _geometryID, m); }
            catch (MagnaundasoniException) { }
        }

        // ----- Gizmos (Editor) ---------------------------------------------
#if UNITY_EDITOR
        private void OnDrawGizmosSelected()
        {
            switch (_dynamicFlag)
            {
                case MagDynamicFlag.Static:           Gizmos.color = new Color(0.5f, 0.5f, 0.5f, 0.3f); break;
                case MagDynamicFlag.QuasiStatic:      Gizmos.color = new Color(0.3f, 0.7f, 1f, 0.3f); break;
                case MagDynamicFlag.DynamicImportant:  Gizmos.color = new Color(1f, 0.6f, 0f, 0.3f); break;
                case MagDynamicFlag.DynamicMinor:      Gizmos.color = new Color(1f, 1f, 0.3f, 0.3f); break;
            }

            var mf = GetComponent<MeshFilter>();
            if (mf != null && mf.sharedMesh != null)
                Gizmos.DrawWireMesh(mf.sharedMesh, transform.position, transform.rotation, transform.lossyScale);
        }
#endif
    }
}
