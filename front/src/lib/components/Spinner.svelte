<script lang="ts">
	interface Props {
		text?: string;
	}

	let { text }: Props = $props();
</script>

{#if typeof text !== 'undefined'}
	<div class="spinner-container">
		<div class="spinner"></div>
		<p>{text}</p>
	</div>
{:else}
	<div class="spinner"></div>
{/if}

<style lang="scss">
	@use './../../lib/styles/variables' as *;

	.spinner-container {
		display: flex;
		align-items: center;
		gap: 12px;

		p {
			margin: 0;
			font-size: 0.875rem;
		}
	}

	.spinner {
		width: 40px;
		height: 40px;
		border: 8px solid rgba($blue-dark, 0.2);
		border-top-color: $blue-dark;
		border-radius: 50%;
		animation: spin 0.8s linear infinite;
	}

	@keyframes spin {
		to {
			transform: rotate(360deg);
		}
	}
</style>
