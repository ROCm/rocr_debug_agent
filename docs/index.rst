.. meta::
   :description: A library that can be loaded by ROCr to print the AMDGPU wavefront states
   :keywords: ROCdebug-agent, ROCm, library, tool, rocr

.. _index:

===============================
ROCR Debug Agent documentation
===============================

The ROCR Debug Agent (ROCdebug-agent) is a library that can be loaded by the ROCm software runtime `(ROCR) <https://rocm.docs.amd.com/projects/ROCR-Runtime/en/latest/>`_ to provide the following functionalities:

- Print the state of all AMDGPU wavefronts that cause a queue error (such as, a memory violation, executing a ``s_trap 2``, or executing an illegal instruction).

- Print the state of all AMDGPU wavefronts by sending a SIGQUIT signal to the process using ``kill -s SIGQUIT <pid>`` command or by pressing ``Ctrl-\``, while the program is executing.

This functionality is provided for all AMDGPUs supported by the ROCm Debugger API Library `(ROCdbgapi) <https://rocm.docs.amd.com/projects/ROCdbgapi/en/latest/>`_.

You can access ROCdebug-agent code on our `GitHub repository <https://github.com/ROCm/rocr_debug_agent>`_.

The documentation is structured as follows:

.. grid:: 2
  :gutter: 3

  .. grid-item-card:: Install

    * :ref:`installation`

  .. grid-item-card:: Conceptual

    * :ref:`user-guide`

To contribute to the documentation, refer to
`Contributing to ROCm  <https://rocm.docs.amd.com/en/latest/contribute/contributing.html>`_.

You can find licensing information on the `Licensing <https://rocm.docs.amd.com/en/latest/about/license.html>`_ page.
